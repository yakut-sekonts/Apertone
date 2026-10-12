# 0001 — Qt 6 / QML desktop
Status: Accepted for M0.

Context: The approved requirements specify C++ and QML on macOS Apple Silicon.

Decision: Qt 6.10.3, C++20 and QML Basic controls. Keep business logic in shared
application services and independent core modules. Official dynamically linked
Qt frameworks are bundled for distribution.

Consequences: A native macOS .app can share its engine with CLI/tests. Qt LGPL
redistribution requirements apply. SwiftUI prototypes are not the implementation.

The SDK version must work with current Apple toolchains. Qt 6.10.3 avoids the
obsolete AGL link dependency present in older 6.8 SDKs on recent macOS SDKs.
