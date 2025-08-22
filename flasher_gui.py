#!/usr/bin/env python3
"""
Simple GUI for flashing pre-compiled firmware with embedded config
"""
import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import json
from pathlib import Path

# Import the flasher class (we'll create this)
try:
    from firmware_flasher_v2 import FirmwareFlasher
except ImportError:
    # Fallback simple flasher for demo
    class FirmwareFlasher:
        def find_rpi_device(self):
            return None
        def load_config(self, name):
            return {"demo": True}
        def flash_firmware(self, firmware_type, config):
            return True

class FirmwareFlasherGUI:
    def __init__(self):
        self.flasher = FirmwareFlasher()
        self.setup_gui()
        
    def setup_gui(self):
        self.root = tk.Tk()
        self.root.title("BGG Firmware Flasher")
        self.root.geometry("500x400")
        
        # Controller Type Selection
        ttk.Label(self.root, text="Firmware Type:").pack(pady=5)
        self.firmware_type = tk.StringVar(value="xinput")
        firmware_frame = ttk.Frame(self.root)
        firmware_frame.pack(pady=5)
        
        ttk.Radiobutton(firmware_frame, text="X-Input (Xbox Compatible)", 
                       variable=self.firmware_type, value="xinput").pack(anchor='w')
        ttk.Radiobutton(firmware_frame, text="HID (Generic)", 
                       variable=self.firmware_type, value="hid").pack(anchor='w')
        
        # Config Selection
        ttk.Label(self.root, text="Configuration:").pack(pady=(20,5))
        config_frame = ttk.Frame(self.root)
        config_frame.pack(pady=5, fill='x', padx=20)
        
        self.config_type = tk.StringVar(value="default")
        ttk.Radiobutton(config_frame, text="Default Settings", 
                       variable=self.config_type, value="default").pack(anchor='w')
        ttk.Radiobutton(config_frame, text="HID Optimized", 
                       variable=self.config_type, value="hid").pack(anchor='w')
        ttk.Radiobutton(config_frame, text="Custom Config File", 
                       variable=self.config_type, value="custom").pack(anchor='w')
        
        # Custom config file selection
        self.custom_config_frame = ttk.Frame(config_frame)
        self.custom_config_frame.pack(fill='x', padx=20)
        
        self.custom_config_path = tk.StringVar()
        ttk.Entry(self.custom_config_frame, textvariable=self.custom_config_path, 
                 state='readonly').pack(side='left', fill='x', expand=True)
        ttk.Button(self.custom_config_frame, text="Browse", 
                  command=self.browse_config).pack(side='right', padx=(5,0))
        
        # Device Detection
        ttk.Label(self.root, text="Device:").pack(pady=(20,5))
        device_frame = ttk.Frame(self.root)
        device_frame.pack(pady=5, fill='x', padx=20)
        
        self.device_status = tk.StringVar(value="No device detected")
        ttk.Label(device_frame, textvariable=self.device_status).pack(side='left')
        ttk.Button(device_frame, text="Refresh", 
                  command=self.refresh_device).pack(side='right')
        
        # Flash Button
        self.flash_button = ttk.Button(self.root, text="Flash Firmware", 
                                     command=self.flash_firmware, state='disabled')
        self.flash_button.pack(pady=20)
        
        # Status
        self.status_text = tk.Text(self.root, height=8, wrap='word')
        self.status_text.pack(pady=10, padx=20, fill='both', expand=True)
        
        # Initial device check
        self.refresh_device()
        
    def browse_config(self):
        filename = filedialog.askopenfilename(
            title="Select Config File",
            filetypes=[("JSON files", "*.json"), ("All files", "*.*")]
        )
        if filename:
            self.custom_config_path.set(filename)
    
    def refresh_device(self):
        device_path = self.flasher.find_rpi_device()
        if device_path:
            self.device_status.set(f"Pico found: {device_path}")
            self.flash_button.config(state='normal')
        else:
            self.device_status.set("No device detected (Put Pico in BOOTSEL mode)")
            self.flash_button.config(state='disabled')
    
    def log_message(self, message):
        self.status_text.insert('end', message + '\n')
        self.status_text.see('end')
        self.root.update()
    
    def load_config_data(self):
        """Load configuration data based on user selection"""
        if self.config_type.get() == "custom":
            custom_path = self.custom_config_path.get()
            if not custom_path:
                raise ValueError("Please select a custom config file")
            with open(custom_path, 'r') as f:
                return json.load(f)
        else:
            return self.flasher.load_config(self.config_type.get())
    
    def flash_firmware(self):
        try:
            self.status_text.delete(1.0, 'end')
            self.log_message("Starting firmware flash...")
            
            # Load configuration
            config_data = self.load_config_data()
            self.log_message(f"Loaded config: {self.config_type.get()}")
            
            # Flash firmware
            firmware_type = self.firmware_type.get()
            self.log_message(f"Flashing {firmware_type} firmware...")
            
            success = self.flasher.flash_firmware(firmware_type, config_data)
            
            if success:
                self.log_message("✓ Firmware flashed successfully!")
                self.log_message("Device will restart automatically.")
                messagebox.showinfo("Success", "Firmware flashed successfully!")
            else:
                self.log_message("✗ Firmware flash failed!")
                messagebox.showerror("Error", "Firmware flash failed!")
                
        except Exception as e:
            error_msg = f"Error: {str(e)}"
            self.log_message(error_msg)
            messagebox.showerror("Error", error_msg)
    
    def run(self):
        self.root.mainloop()

def main():
    app = FirmwareFlasherGUI()
    app.run()

if __name__ == "__main__":
    main()
