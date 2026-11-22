"""Exercise the supported WebDAV contract using only a generated local gallery."""

import base64
from contextlib import closing
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
from threading import Thread
from urllib.parse import unquote, urlsplit


USER = "apertone-ci"
PASSWORD = "local-fixture"
AUTHORIZATION = "Basic " + base64.b64encode(f"{USER}:{PASSWORD}".encode()).decode()


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
        if Path(path).parent.as_posix() != "/dav" or not Path(path).name.startswith(".apertone-upload-"):
            self.status(409)
            return
        if path in self.server.folders:
            self.status(405)
            return
        self.server.folders[path] = {}
        self.status(201)

    def do_PUT(self):
        if not self.authorized():
            return
        directory, _, filename = unquote(self.path).rpartition("/")
        body = self.rfile.read(int(self.headers["Content-Length"]))
        if directory not in self.server.folders or not body or self.server.fail_put:
            self.status(500)
            return
        self.server.folders[directory][filename] = body
        self.status(201)

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
    assert json.loads(run("publish-webdav", endpoint, folder, "Тест <Apertone>", original).stdout) == {"published": True}
    gallery = dav.folders[f"/dav/{folder}"]
    assert set(gallery) == {"index.html", "photo-0001.jpg"} and not stages()
    assert "Тест &lt;Apertone&gt;" in gallery["index.html"].decode()
    assert gallery["photo-0001.jpg"].startswith(b"\xff\xd8")

    failed = run("publish-webdav", endpoint, folder, "Duplicate", original, exit_code=1)
    assert "already exists" in failed.stderr and not stages() and dav.folders[f"/dav/{folder}"] == gallery
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
