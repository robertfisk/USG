# Introduction

USB is the universal peripheral interface for personal computers. For over 15 years USB has provided an easy-to-use interface compatible with a wide variety of hardware devices. But this long legacy and wide compatibility also bring security risks, which are now coming to be understood.

To achieve its wide compatibility, USB employs a complex layered protocol. This requires every USB device to incorporate a microprocessor (embedded CPU) to send & receive commands and data. And naturally, these microprocessors need to run firmware.

In simpler times the host computer and the USB device trusted each other, and so USB implementations historically placed little emphasis on security issues. But what if malicious firmware were loaded into a device? How much damage could it do?

# BadUSB
Any USB device can be 'turned bad' by loading malicious firmware into its microprocessor, either during the manufacturing process or out in the field. Broadly speaking there are two classes of attack a BadUSB device can perform against a host computer.

1. Protocol exploit attacks: A device could supply maliciously malformed data to USB software running on the host computer. This could gain code execution on the host through a buffer overflow or other vulnerability in the USB host stack. The malicious device could also choose to communicate with any of the hundreds of USB device drivers found on a typical personal computer. These device drivers run with system priveliges, are often developed by third parties, and are rarely tested against unexpected or malformed input.

2. Protocol-compliant attacks: Due to USB's flexibility and wide compatibility, a BadUSB device does not have to exploit any actual software bug to achieve its nefarious aim. It is quite possible to perform malicious actions against a host computer using 100% USB-protocol-compliant commands. By way of example, a BadUSB device might emulate a keyboard or mouse, and enter any command the user is authorised to perform. It might emulate a mass storage device with a virus pre-loaded. It might emulate a network card and redirect the user's traffic. The possibilities are limited only by an attacker's imagination and because only legitimate USB commands are used, concerned users have no way of defending themselves. That is, until now.

# The USG is Good, not Bad
The USG was created to solve the problem of BadUSB. It is a device that you insert between your computer and your untrusted USB device, and it ensures that no malicious commands or data can pass between them.

The USG operates on the principle of 'security by isolation'. It uses two USB-enabled microprocessors: one connects to your untrusted device while the other emulates your device to the host computer. These microprocessors exchange data through a simple protocol running over a simple serial interface - SPI.

This inter-microprocessor interface protocol has a much smaller attack surface than a USB software stack, so it is simple enough for us to guarantee security by correctness. So even in the worst-case-scenario of a BadUSB compromising the firmware in one microprocessor we can guarantee that the second microprocessor cannot be compromised, and thus your host computer is safe.

This security-by-isolation approach protects primarily against protocol exploits (type 1 above). To protect against protocol-compliant attacks (type 2) we can add some rules into our USG firmware to ensure devices don't try to misbehave.

- Only one device attached at a time: This limitation of the embedded USB firmware and hardware is also a feature. By supporting only one attached device, we eliminate attacks that use a hidden hub and additional device to perform unexpected actions.

- No run-time device class changes: Once a USB device is enumerated through the USG, it cannot re-enumerate itself as a different device class until power is removed and reapplied to the USG. This blocks attacks where a device unexpectedly changes its functionality to perform malicious actions.

- Class-specific rules: As support for more device classes is added to the USG firmware, rules specific to a particular class can be added. For example, the mass storage class might not support unusual block sizes due to the risk of exploits in poorly tested code paths on the host. Or the human interface device (HID) class might block keystrokes arriving faster than a reasonable human typing speed.

# Project Status
The USG is currently in beta, so calibrate your expectations accordingly!

## Hardware
The first generation of USG hardware supports full-speed USB only, i.e. 12Mbps. If it proves sufficiently popular a high-speed 480Mbps version may be developed later.

As the USG is still in beta, no actual hardware or designs are available. If you want to get your hands on one now, you will have to build one yourself out of development boards. See the wiki for more details.

## Firmware
The USG firmware currently supports mass storage devices only. HID support is planned for the near future, in between hardware development efforts.

