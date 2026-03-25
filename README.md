# Serial Communication with PLC - C Example

This is a C-based example of how to communicate with a PLC such as Triangle Research's WX100 via an RS485 connection. This has been tested on Linux on both an Intel x86 laptop and a Raspberry Pi 4 Model B.

No additional libraries are required to run this program. Any standard Linux distribution should already ship with everything you need.

## Usage

1. In the `main()` function of `main.c`, look for the comment `// default` and adjust the values of the 6 lines underneath to suit your particular setup.
2. Compile `main.c` any way you want (e.g. by running `gcc main.c`)
3. Execute the generated output file (e.g. `./a.out`)
4. Start sending serial commands (enter `exit` to close the program).

## Notes
* If you wish to send [Hostlink](https://docs.triplc.com/hostlink/) commands to a Triangle Research PLC like the WX100, you need to format your commands for the [multi-point](https://docs.triplc.com/um/wx100-users-manual/chapter-11-20/wx100-chapter-15/15-2/) protocol. Every command must begin with an `@` character.