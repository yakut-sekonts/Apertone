"""Exercise the supported WebDAV contract using only a generated local gallery."""

import base64
from contextlib import closing
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import tempfile
from threading import Thread
from urllib.parse import unquote, urlsplit


USER = "apertone-ci"
PASSWORD = "local-fixture"
AUTHORIZATION = "Basic " + base64.b64encode(f"{USER}:{PASSWORD}".encode()).decode()


def etag(body):
    return '"' + hashlib.sha256(body).hexdigest() + '"'


class DavFixture(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def status(self, code):
        self.send_response(code)
        self.end_headers()

    def authorized(self):
        self.server.requests.append((self.command, unquote(self.path)))
        if self.headers.get("Authorization") == AUTHORIZATION:
            return True
        self.send_response(401)
        self.send_header("WWW-Authenticate", 'Basic realm="fixture"')
        self.end_headers()
        return False

    def do_MKCOL(self):
        if not self.authorized():
            return
        if self.server.redirect:
            self.send_response(307)
            self.send_header("Location", "/redirected")
            self.end_headers()
            return
        path = unquote(self.path)
        parent = Path(path).parent.as_posix()
        if (parent != "/dav" and parent not in self.server.folders) or not Path(path).name.startswith(".apertone-upload-"):
            self.status(409)
            return
        if path in self.server.folders:
            self.status(405)
            return
        self.server.folders[path] = {}
        self.status(201)

    def do_GET(self):
        if not self.authorized():
            return
        directory, _, filename = unquote(self.path).rpartition("/")
        body = self.server.folders.get(directory, {}).get(filename)
        if body is None:
            self.status(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        if self.server.etag_mode:
            self.send_header("ETag", ("W/" if self.server.etag_mode == "weak" else "") + etag(body))
        self.end_headers()
        self.wfile.write(body)

    def do_PUT(self):
        if not self.authorized():
            return
        directory, _, filename = unquote(self.path).rpartition("/")
        body = self.rfile.read(int(self.headers["Content-Length"]))
        if directory not in self.server.folders or not body or self.server.fail_put:
            self.status(500)
            return
        files = self.server.folders[directory]
        condition = self.headers.get("If-Match")
        if (self.server.concurrent_edit is not None and condition and filename == "index.html"
                and not Path(directory).name.startswith(".apertone-upload-")):
            files[filename] = self.server.concurrent_edit
            self.server.concurrent_edit = None
        existing = files.get(filename)
        if condition and not self.server.ignore_conditions and (existing is None or condition != etag(existing)):
            # HTTP permits a successful response if an equivalent write already took effect.
            self.status(204 if body == existing else 412)
            return
        files[filename] = body
        self.status(201 if existing is None else 204)

    def do_MOVE(self):
        if not self.authorized():
            return
        destination = urlsplit(self.headers["Destination"])
        path = unquote(destination.path)
        if destination.netloc != self.headers["Host"] or self.headers["Overwrite"] != "F":
            self.status(400)
        elif path in self.server.folders:
            self.status(412)
        else:
            self.server.folders[path] = self.server.folders.pop(unquote(self.path))
            self.status(201)

    def do_DELETE(self):
        if not self.authorized():
            return
        path = unquote(self.path)
        if Path(path).name.startswith(".apertone-upload-"):
            self.server.folders.pop(path, None)
            self.status(204)
        else:
            self.status(403)


def server():
    instance = ThreadingHTTPServer(("127.0.0.1", 0), DavFixture)
    instance.requests = []
    instance.folders = {}
    instance.fail_put = False
    instance.redirect = False
    instance.etag_mode = "strong"
    instance.ignore_conditions = False
    instance.concurrent_edit = None
    return instance


def verify(binary, directory, dav):
    catalog = directory / "Paired.sqlite"
    fixture = Path(__file__).resolve().parents[1] / "assets/migration-051/01-complete.DNG"
    endpoint = f"http://127.0.0.1:{dav.server_port}/dav/"
    env = dict(os.environ, APERTONE_WEBDAV_USER=USER, APERTONE_WEBDAV_PASSWORD=PASSWORD)

    def run(*args, exit_code=0, environment=env):
        result = subprocess.run([binary, *map(str, args), "--catalog", catalog],
                                capture_output=True, encoding="utf-8", errors="replace",
                                timeout=45, env=environment)
        assert result.returncode == exit_code, (result.stdout, result.stderr)
        return result

    original = json.loads(run("open", fixture).stdout)["asset"]
    stages = lambda: {path for path in dav.folders if "/.apertone-upload-" in path}
    folder = "Альбом для проверки"
    published_path = f"/dav/{folder}"
    assert json.loads(run("publish-webdav", endpoint, folder, "Тест <Apertone>", original).stdout) == {"published": True}
    gallery = dav.folders[published_path]
    first_page = gallery["index.html"]
    first_photo = gallery["photo-0001.jpg"]
    assert set(gallery) == {"index.html", "photo-0001.jpg"} and not stages()
    assert "Тест &lt;Apertone&gt;" in gallery["index.html"].decode()
    assert gallery["photo-0001.jpg"].startswith(b"\xff\xd8")

    failed = run("publish-webdav", endpoint, folder, "Duplicate", original, exit_code=1)
    assert "already exists" in failed.stderr and not stages() and gallery["index.html"] == first_page

    # Even a byte-for-byte clone is not a publication receipt for another URL.
    dav.folders["/dav/Foreign"] = dict(gallery)
    before = len(dav.requests)
    failed = run("update-webdav", endpoint, "Foreign", "Foreign", original, exit_code=1)
    assert "published by this catalog" in failed.stderr
    assert dav.requests[before:] == [("GET", "/dav/Foreign/index.html")]

    for mode in (None, "weak"):
        dav.etag_mode = mode
        before = len(dav.requests)
        failed = run("update-webdav", endpoint, folder, "No strong ETag", original, exit_code=1)
        assert "does not support safe" in failed.stderr
        assert dav.requests[before:] == [("GET", f"{published_path}/index.html")]
    dav.etag_mode = "strong"

    dav.fail_put = True
    run("update-webdav", endpoint, folder, "Interrupted update", original, exit_code=1)
    dav.fail_put = False
    assert gallery["index.html"] == first_page and not stages()

    dav.ignore_conditions = True
    failed = run("update-webdav", endpoint, folder, "Ignore conditions", original, exit_code=1)
    dav.ignore_conditions = False
    assert "does not support safe" in failed.stderr and not stages()
    assert gallery["index.html"] == first_page and not any("/apertone-revision-" in p for p in dav.folders)

    assert json.loads(run("update-webdav", endpoint, folder, "Updated <Apertone>", original).stdout) == {"published": True}
    updated_page = gallery["index.html"]
    generation = re.search(rb'href="(apertone-revision-[a-f0-9]{32})/photo-0001.jpg"', updated_page)
    assert generation is not None and b"Updated &lt;Apertone&gt;" in updated_page and not stages()
    revision = dav.folders[f"{published_path}/{generation[1].decode()}"]
    assert set(revision) == {"index.html", "photo-0001.jpg", "previous.html"}
    assert revision["previous.html"].replace(b'<base href="../">', b"") == first_page
    assert gallery["photo-0001.jpg"] == first_photo

    # A second update can authenticate the current generation and archive its landing page.
    run("update-webdav", endpoint, folder, "Third gallery", original)
    third_page = gallery["index.html"]
    assert b"Third gallery" in third_page and third_page != updated_page

    # Simulate an independent write between the GET and the conditional PUT.
    dav.concurrent_edit = b"A gallery changed by another editor"
    failed = run("update-webdav", endpoint, folder, "Competing gallery", original, exit_code=1)
    assert "changed before the update" in failed.stderr and not stages()
    assert gallery["index.html"] == b"A gallery changed by another editor"
    before = len(dav.requests)
    failed = run("update-webdav", endpoint, folder, "Manual edit", original, exit_code=1)
    assert "published by this catalog" in failed.stderr
    assert dav.requests[before:] == [("GET", f"{published_path}/index.html")]

    dav.fail_put = True
    run("publish-webdav", endpoint, "Interrupted", "Interrupted", original, exit_code=1)
    dav.fail_put = False
    assert not stages() and "/dav/Interrupted" not in dav.folders

    wrong = dict(env, APERTONE_WEBDAV_PASSWORD="wrong")
    failed = run("publish-webdav", endpoint, "Bad-auth", "Bad auth", original, exit_code=1, environment=wrong)
    assert "authorization failed" in failed.stderr and not stages()
    dav.redirect = True
    failed = run("publish-webdav", endpoint, "Redirect", "Redirect", original, exit_code=1)
    assert "final WebDAV server URL" in failed.stderr and not stages()
    assert not any(path == "/redirected" for _, path in dav.requests)
    before = len(dav.requests)
    failed = run("publish-webdav", "http://example.invalid/dav/", "Insecure", "Insecure", original, exit_code=1)
    assert "HTTPS" in failed.stderr and len(dav.requests) == before
    with closing(sqlite3.connect(catalog)) as db:
        assert db.execute("PRAGMA integrity_check").fetchone() == ("ok",)


if __name__ == "__main__":
    binary = Path(sys.argv[1]).resolve(strict=True)
    with server() as dav, tempfile.TemporaryDirectory(prefix="apertone-webdav-") as directory:
        worker = Thread(target=dav.serve_forever, daemon=True)
        worker.start()
        try:
            verify(binary, Path(directory), dav)
        finally:
            dav.shutdown()
            worker.join()
