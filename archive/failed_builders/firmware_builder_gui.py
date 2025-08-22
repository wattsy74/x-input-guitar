
import tkinter as tk
from tkinter import ttk, messagebox
import firmware_builder

class FirmwareBuilderApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('Firmware Builder & Uploader')
        self.geometry('400x250')
        self.controller_type = tk.StringVar(value='X-Input')
        self.port = tk.StringVar()
        self.create_widgets()

    def create_widgets(self):
        ttk.Label(self, text='Controller Type:').pack(pady=10)
        controller_menu = ttk.Combobox(self, textvariable=self.controller_type, values=list(firmware_builder.FIRMWARE_FILES.keys()), state='readonly')
        controller_menu.pack()

        ttk.Label(self, text='Serial Port:').pack(pady=10)
        port_entry = ttk.Entry(self, textvariable=self.port)
        port_entry.pack()
        ttk.Label(self, text='(e.g., COM3)').pack()

        ttk.Button(self, text='Compile', command=self.compile_firmware).pack(pady=10)
        ttk.Button(self, text='Upload', command=self.upload_firmware).pack(pady=5)

    def compile_firmware(self):
        try:
            msg = firmware_builder.compile_firmware(self.controller_type.get())
            messagebox.showinfo('Success', msg)
        except Exception as e:
            messagebox.showerror('Compile Error', str(e))

    def upload_firmware(self):
        try:
            msg = firmware_builder.upload_firmware(self.controller_type.get(), self.port.get())
            messagebox.showinfo('Success', msg)
        except Exception as e:
            messagebox.showerror('Upload Error', str(e))

if __name__ == '__main__':
    app = FirmwareBuilderApp()
    app.mainloop()
