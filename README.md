# ELB Lamp

ESP32:  
• ESP32 module used: ESP32-WROOM-32.  
• ESP32 chip in module: ESP32-D0WDQ6 (revision v1.0).  
• Module has 4MB flash ROM.  

Building:  
• Get command prompt using shortcut: "ESP-IDF CMD"  
• Command: idf.py all  

Programming:  
• Get command prompt using shortcut: "ESP-IDF CMD"  
• Connect ESP32 USB connector to PC.  
• Power lamp with 12V.  
• Press and hold BOOT button.  
• Command: idf --port COMx flash  
• To program firmware from the Repository folders, use esptool.py or "ESP FLASH DOWNLOAD TOOL" (GUI) to program firmware bin file to address 0x10000 (I think that's what "idf.py flash" does).  

Monitoring:  
• Get command prompt using shortcut: "ESP-IDF CMD"  
• Press and hold BOOT button.  
• Command: idf --port COMx monitor  

JTAG debugging:  
• General notes:  
  • The basic principle is to get OpenOCD running in one esp-idf terminal, and GDB running in another one.
• Help: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/jtag-debugging/index.html
• Device: FT4232 (Quad) 
• Use Zaqig to set "Quad RS232HS (Interface 0)" to use driver "WinUSB (v6.1.7600.16385)".  
• Connections from lamp to device:  
  • J1-1 (GND) <=> GND  
  • J1-2 (VTRef) <=> No connection  
  • J1-3 (GPIO13 = TCK) <=> ADBUS0 (TCK)  
  • J1-4 (GPIO12 = TDI) <=> ADBUS1 (TDI)  
  • J1-5 (GPIO15 = TDO) <=> ADBUS2 (TDO)  
  • J1-6 (GPIO14 = TMS) <=> ADBUS1 (TMS)  
  • J1-7 (EN = nTRST) <=> [No connection]  
  • J1-8
• Get command prompt using shortcut: "ESP-IDF CMD"  
• Command: idf.py -v openocd  
• Command: xtensa-esp32-elf-gdb -q -x build/gdbinit/symbols -x build/gdbinit/prefix_map -x build/gdbinit/connect build/Emma_DT_lamp.elf  
• Commands:  
  • Breakpoints:  
    • Set: break <FunctionName>|<LineNumber|<FileName:LineNumber>  
      • Examples:  
        • break WiFi_Initialize  
        • break 198  
    • Clear: clear <Function name>|<Line number>  
  • Go: continue     [Not run!]  
  • See source code: list
  • Step over: n  
  • Step into: s  
  • Continue to calling code of current function: finish  

WiFi debugging:  
• Use "idf.py menuconfig" to set default log verbosity to "Info".
  • Component config => Log => Log level => Default log verbosity => Info