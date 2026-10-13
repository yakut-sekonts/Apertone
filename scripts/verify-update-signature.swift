import CryptoKit
import Foundation

// Public data only: archive path, Ed25519 public key and detached signature.
guard CommandLine.arguments.count == 4,
      let key = Data(base64Encoded: CommandLine.arguments[2]),
      let signature = Data(base64Encoded: CommandLine.arguments[3]) else {
    exit(1)
}
do {
    let publicKey = try Curve25519.Signing.PublicKey(rawRepresentation: key)
    let archive = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]), options: .mappedIfSafe)
    guard publicKey.isValidSignature(signature, for: archive) else { exit(1) }
} catch {
    exit(1)
}
