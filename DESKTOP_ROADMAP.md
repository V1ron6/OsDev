# ByteBandit Desktop Roadmap

## Vision

ByteBandit will keep Linux-like process, filesystem, permissions, scripting, and
hardware behavior while presenting a polished desktop inspired by macOS:
spatial desktop navigation, a top system bar, a dock, windows with predictable
controls, search, notifications, and keyboard-driven workflows.

The visual language may be macOS-inspired, but the operating model remains a
hybrid: Unix-like paths and permissions, explicit process identities, scriptable
commands, and Windows-like handles where they make the API clearer.

## Current Starting Point

The project currently has:

- x86 protected-mode kernel and bootloader
- paging, heap, task metadata, and syscall scaffolding
- persistent ATA-backed ByteFS storage with MBR partition discovery
- inode metadata, permissions, CRC32 validation, and alternating volume slots
- serial command console with power and filesystem utilities
- versioned ISO builds through `builder.sh`
- one-command QEMU execution through `run.sh`

The GUI must wait until user processes, memory isolation, input devices, and
filesystem semantics are reliable. The desktop should be a user-space consumer
of stable kernel services, not a second kernel subsystem with special rules.

## Stage 1: Kernel and Process Stability

Before graphics:

- Complete timer-driven context switching.
- Complete ring-3 ELF launch and return paths.
- Implement safe `exit`, `wait`, and `spawn` behavior.
- Move file descriptors from one global table to per-process tables.
- Add page-fault and general-protection-fault recovery.
- Add process groups, sessions, signals, and basic job control.
- Add stable user/kernel ABI versioning for GUI services.

## Stage 2: Hardware and Input Foundation

- Add a keyboard driver with scancode translation.
- Add modifier, key-repeat, and compose-key handling.
- Add a mouse or PS/2 pointing-device driver.
- Define input events independent of the serial console.
- Add a device registry and clean driver ownership boundaries.
- Keep serial input available as a recovery and debugging console.

## Stage 3: Graphics and Display

Start with the simplest reliable display path:

- Add a framebuffer or VBE boot mode.
- Define a pixel format and framebuffer mapping API.
- Add a software renderer for rectangles, text, icons, and bitmaps.
- Add clipping, dirty rectangles, and double buffering.
- Add a font format and text shaping baseline.
- Add display modes and a fallback VGA text mode.
- Add screenshot and framebuffer diagnostics to the debug workflow.

## Stage 4: Window Server

Create a privileged user-space display service rather than putting windows in
the kernel:

- Define window, surface, buffer, and event handles.
- Add client registration and authenticated ownership.
- Composite application surfaces into the framebuffer.
- Route keyboard and pointer events to the focused window.
- Implement focus, stacking, resize, move, minimize, maximize, and close.
- Add per-client damage tracking and frame throttling.
- Make a crashed client destroy only its own windows and resources.

## Stage 5: Desktop Shell

Build the first usable desktop environment:

- Top bar with time, system state, network, volume, and session controls.
- Dock with pinned and running applications.
- Window decorations with macOS-inspired traffic-light controls.
- Workspace overview and virtual desktops.
- Application launcher and keyboard search.
- Notifications and transient dialogs.
- File manager backed by ByteFS paths and permissions.
- Terminal application backed by the existing shell service.
- Settings application backed by the persistent registry.
- Login/session manager with UID/GID selection and permission boundaries.

## Stage 6: Application and Service APIs

- Versioned GUI syscall or IPC services for windows, events, surfaces, and
  clipboard access.
- Shared-memory buffers for efficient application rendering.
- Timers, async events, pipes, and service discovery.
- Clipboard, drag-and-drop, file open/save dialogs, and desktop notifications.
- Application bundles or executable metadata for launcher discovery.
- Resource ownership and cleanup on process exit.
- Accessibility hooks: keyboard navigation, scaling, contrast, and screen
  reader/event metadata foundations.

## Stage 7: Desktop Reliability and Polish

- Persistent desktop preferences and per-user settings.
- Crash recovery and application relaunch options.
- Safe mode and text-console fallback.
- Power management integrated with `shutdown`, `reboot`, and session logout.
- Performance tracing for compositor, input latency, and frame timing.
- Theme, wallpaper, icon, font, and appearance asset packaging.
- Installer/recovery image and upgrade-safe filesystem migration.

## Definition of Done for the First GUI Release

- Boots to a graphical login or local desktop session.
- Terminal and file manager can create, read, write, and delete permitted files.
- At least two GUI applications can run concurrently without corrupting each
  other or the compositor.
- Keyboard and pointer input work in both GUI and recovery console modes.
- A crashed application does not crash the window server or kernel.
- User settings survive reboot on the persistent ByteFS partition.
- `run.sh`, the serial console, and GDB remain available for diagnostics.
- Every public GUI service is documented in `BYTEBANDIT_API.md` before release.

## Current Priority

Do not begin desktop rendering yet. The immediate order is:

1. Process context switching and safe task exit.
2. User process APIs and per-process resources.
3. Keyboard/mouse input and device abstractions.
4. Framebuffer and software rendering.
5. Window server and compositor.
6. Desktop shell and applications.
