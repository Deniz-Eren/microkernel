# Glossary

| Term | Definition |
| --- | --- |
| Need | A single uniquely identified engineering item in Sphinx-Needs. |
| Domain | An uppercase `[A-Z0-9]+` subsystem identifier used in item IDs. |
| Forward link | A relationship recorded once on the lower or derived item. |
| Channel | A kernel receive endpoint owned by one Ring 3 process (`chanid`). |
| Connection | A client attachment (`connid`) to a `(pid, chanid)` endpoint. |
| Pulse | A fixed 8-byte non-blocking notification sent over a connection. |
| HHDM | Higher-Half Direct Map (`0xFFFF800000000000`) of physical RAM. |
| W^X | Write XOR Execute: no page is both writable and executable. |
| POSIX.1-2024 | IEEE Std 1003.1-2024 (Issue 8), target Base OS standard. |
