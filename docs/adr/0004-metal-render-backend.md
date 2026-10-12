# 0004 — CPU reference before Metal image processing
Status: Accepted; Metal processing implementation planned for 0.6.

Context: macOS Metal is the required GPU direction, with a CPU reference path.

Decision: Implement and test Float32 Exposure on CPU first. Qt's macOS RHI may
use Metal for presentation; that is not claimed as GPU image processing. A later
Metal backend must satisfy numerical/image quality tolerance tests against CPU.

Consequences: M0 validates the workflow before optimization. No Vulkan, CUDA or
Direct3D abstractions are added. Performance goals remain unverified until measured.
