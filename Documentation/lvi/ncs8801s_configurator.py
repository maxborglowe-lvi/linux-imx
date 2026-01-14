import tkinter as tk
from tkinter import ttk, filedialog
import re

class NCS8801SConfigurator:
    def __init__(self, root):
        self.root = root
        self.root.title("NCS8801S Configurator")
        self.root.geometry("800x600")

        self.registers = {
            0x00: {
                "name": "Input options",
                "fields": [
                    {
                        "name": "Input", 
                        "options": ["LVDS 2-channel", "LVDS 1-channel", "RGB"], 
                        "bits": (0, 3),
                        "value_map": {
                            "LVDS 2-channel": 0,  # Binary: 000
                            "LVDS 1-channel": 1,  # Binary: 001
                            "RGB": 4,            # Binary: 100
                        }
                    },
                ],
                "default": 0x00
            },
            0x02: {
                "name": "TX/RX Parameter adaptive disable",
                "fields": [
                    {
                        "name": "RX panel parameter adaptive",
                        "options": ["Disable", "Enable"],
                        "bits": (1,2),
                        "value_map": {
                            "Disable": 0,  # Binary: 01
                            "Enable": 1,  # Binary: 00
                        }
                    },
                    {
                        "name": "TX panel parameter adaptive",
                        "options": ["Disable", "Enable"],
                        "bits": (2,3),
                        "value_map": {
                            "Disable": 0,  # Binary: 01
                            "Enable": 1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x00
            },
            0x03: {
                "name": "LVDS Control",
                "fields": [
                    {
                        "name": "a/b Channel Swap",
                        "options": ["a->a, b->b", "a->b, b->a"],
                        "bits": (0, 2),
                        "value_map": {
                            "a->a, b->b": 0,  # Binary: 000
                            "a->b, b->a": 3,  # Binary: 001
                        }
                    },
                    {
                        "name": "LVDS Mode",
                        "options": ["VESA", "JEIDA"],
                        "bits": (2, 3),
                        "value_map": {
                            "VESA": 0,  # Binary: 01
                            "JEIDA": 1,  # Binary: 00
                        }
                    },
                    {
                        "name": "LVDS Polarity",
                        "options": ["Normal", "Polarity Swap"],
                        "bits": (3, 4),
                        "value_map": {
                            "Normal": 0,  # Binary: 01
                            "Polarity Swap": 1,  # Binary: 00
                        }
                    }
                ],
                "default": 0x00
            },
            0x07: {
                "name": "eDP TX Control",
                "fields": [
                    {
                        "name": "TX Lane Mode",
                        "options": ["1 Lane", "2 Lane", "4 Lane"],
                        "bits": (0, 3),
                        "value_map": {
                            "1 Lane": 1,
                            "2 Lane": 2,
                            "4 Lane": 4,
                        }
                    },
                    {
                        "name": "Data Rate",
                        "options": ["HBR", "RBR"],
                        "bits": (4, 5),
                        "value_map": {
                            "HBR": 0,  # Binary: 01
                            "RBR": 1,  # Binary: 00
                        }
                    },
                    {
                        "name": "Pixel Bit Width",
                        "options": ["24-bit", "18-bit"],
                        "bits": (5, 6),
                        "value_map": {
                            "24-bit": 0,  # Binary: 01
                            "18-bit": 1,  # Binary: 00
                        }
                    },
                    {
                        "name": "Hardware Training",
                        "options": ["Disable", "Enable"],
                        "bits": (6, 7),  
                        "value_map": {
                            "Disable": 0,  # Binary: 01
                            "Enable": 1,  # Binary: 00
                        }
                    },
                    {
                        "name": "eDP TX Data Pattern",
                        "options": ["Disable", "Enable"],
                        "bits": (7, 8),  
                        "value_map": {
                            "Disable": 0,  # Binary: 01
                            "Enable": 1,  # Binary: 00
                        }
                    }
                ],
                "default": 0x14
            },
            0x09: {
                "name": "eDP TX Port Number",
                "fields": [
                    {
                        "name": "TX Port Num",
                        "options": ["1 port", "2 port",
                                    "4 port"],
                        "bits": (0, 3),  # 3 bits [2:0]
                        "value_map": {
                            "1 port": 1,  # Binary: 001
                            "2 port": 2,  # Binary: 001 NOT IN SPEC
                            "4 port": 4,  # Binary: 100
                        }
                    }
                ],
                "default": 0x04
            },
            0x0B: {
                "name": "BIST Control",
                "fields": [
                    {
                        "name": "BIST Enable",
                        "options": ["bist disable", "bist enable"],
                        "bits": (0, 1),  # Bit 0
                        "value_map": {
                            "bist disable": 0,  # Binary: 0
                            "bist enable": 1,  # Binary: 1
                        }
                    },
                    {
                        "name": "BIST Pattern",
                        "options": ["no pattern", "pattern 1", "pattern 2", "pattern 3", "pattern 4", "pattern 5", "pattern 6", "pattern 7", "pattern 8"],  # Patterns can be added as needed
                        "bits": (4, 8),  # Bits [7:4]
                        "value_map": {
                            "no pattern": 0,
                            "pattern 1": 1,
                            "pattern 2": 2,
                            "pattern 3": 3,
                            "pattern 4": 4,
                            "pattern 5": 5,
                            "pattern 6": 6,
                            "pattern 7": 7,
                            "pattern 8": 8,
                        }
                    }
                ],
                "default": 0x00
            },
            0x0C: {
                "name": "BIST R Component",
                "fields": [
                    {
                        "name": "R Component",
                        "bits": (0, 8),  # Full byte
                        "default_value": 255,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0,  # Minimum allowed value
                        "max_value": 255   # Maximum allowed value
                    }
                ],
                "default": 255
            },
            0x0D: {
                "name": "BIST G Component",
                "fields": [
                    {
                        "name": "G Component",
                        "bits": (0, 8),  # Full byte
                        "default_value": 0,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0,  # Minimum allowed value
                        "max_value": 255   # Maximum allowed value
                    }
                ],
                "default": 0
            },
            0x0E: {
                "name": "BIST B Component",
                "fields": [
                    {
                        "name": "R Component",
                        "bits": (0, 8),  # Full byte
                        "default_value": 0,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0,  # Minimum allowed value
                        "max_value": 255   # Maximum allowed value
                    }
                ],
                "default": 0
            },
            0x10: {
                "name": "Input Htotal (Hsync + HBP + Hactive + HFP)",
                "fields": [
                    {
                        "name": "Input Htotal",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x08A0,  # 2208 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x08A0
            },
            0x12: {
                "name": "Input Hstart (Hsync + HBP)",
                "fields": [
                    {
                        "name": "Input Hstart",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0050,  # 80 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0050
            },
            0x14: {
                "name": "Input Hactive (Hactive)",
                "fields": [
                    {
                        "name": "Input Hactive",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0780,  # 2432 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0780
            },
            0x16: {
                "name": "Input Vtotal (Vsync + VBP + Vactive + VFP)",
                "fields": [
                    {
                        "name": "Input Vtotal",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0465,  # 32 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0465
            },
            0x18: {
                "name": "Input Vstart (Vsync + VBP)",
                "fields": [
                    {
                        "name": "Input Vstart",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x000C,  # 1200 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x000C
            },
            0x1A: {
                "name": "Input Vactive (Vactive)",
                "fields": [
                    {
                        "name": "Input Vactive",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0438,  # 32 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0438
            },
            0x1C: {
                "name": "Input Hsync Polarity Control",
                "fields": [
                    {
                        "name": "Polarity", 
                        "options": ["Active High", "Active Low"], 
                        "bits": (7, 8),
                        "value_map": {
                            "Active High": 0,  # Binary: 01
                            "Active Low": 1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x80
            },  
            0x1D: {
                "name": "Input Hsync Width/Length (Hsync)",
                "fields": [
                    {
                        "name": "Hsync Width", 
                        "default_value": 0x10,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0x00,  # Minimum allowed value
                        "max_value": 0xFF,   # Maximum allowed value
                        "bits": (0, 8),
                    },
                ],
                "default": 0x10
            },  
            0x1E: {
                "name": "Input Vsync Polarity Control",
                "fields": [
                    {
                        "name": "Polarity", 
                        "options": ["Active High", "Active Low"], 
                        "bits": (7, 8),
                        "value_map": {
                            "Active High": 0,  # Binary: 01
                            "Active Low": 1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x80
            },  
            0x1F: {
                "name": "Input Vsync Width/Length (Vsync)",
                "fields": [
                    {
                        "name": "Vsync Width", 
                        "default_value": 0x03,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0x00,  # Minimum allowed value
                        "max_value": 0xFF,   # Maximum allowed value
                        "bits": (0, 8),
                    },
                ],
                "default": 0x03
            },
            0x20: {
                "name": "Output Htotal (Hsync + HBP + Hactive + HFP)",
                "fields": [
                    {
                        "name": "Output Htotal",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x1040,  # 512 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x1040
            },
            0x22: {
                "name": "Output Hstart (Hsync + HBP)",
                "fields": [
                    {
                        "name": "Output Hstart",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x00C0,  # 16 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x00C0
            },
            0x24: {
                "name": "Output Hactive (Hactive)",
                "fields": [
                    {
                        "name": "Output Hactive",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0F00,  # 16 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0F00
            },
            0x26: {
                "name": "Output Vtotal (Vsync + VBP + Vactive + VFP)",
                "fields": [
                    {
                        "name": "Output Vtotal",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x08AE,  # 512 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x08AE
            },
            0x28: {
                "name": "Output Vstart (Vsync + VBP)",
                "fields": [
                    {
                        "name": "Output Vstart ",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x002C,  # 16 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x002C
            },
            0x2A: {
                "name": "Output Vactive (Vactive)",
                "fields": [
                    {
                        "name": "Output Vactive",
                        "bits": (0, 16),  # 16-bit value
                        "default_value": 0x0870,  # 16 in decimal
                        "input_type": "numeric",  # Numeric input (0x0000 to 0xFFFF)
                        "min_value": 0x0000,
                        "max_value": 0xFFFF
                    }
                ],
                "default": 0x0870
            },
            0x2C: {
                "name": "Output Hsync Polarity Control",
                "fields": [
                    {
                        "name": "Polarity", 
                        "options": ["Active High", "Active Low"], 
                        "bits": (7, 8),
                        "value_map": {
                            "Active High": 0x0,  # Binary: 01
                            "Active Low": 0x1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x80
            },  
            0x2D: {
                "name": "Output Hsync Width/Length",
                "fields": [
                    {
                        "name": "Hsync Width", 
                        "default_value": 0x20,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0x00,  # Minimum allowed value
                        "max_value": 0xFF,   # Maximum allowed value
                        "bits": (0, 8),
                    },
                ],
                "default": 0x20
            },  
            0x2E: {
                "name": "Output Vsync Polarity Control",
                "fields": [
                    {
                        "name": "Polarity", 
                        "options": ["Active High", "Active Low"], 
                        "bits": (7, 8),
                        "value_map": {
                            "Active High": 0x0,  # Binary: 01
                            "Active Low": 0x1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x80
            },  
            0x2F: {
                "name": "Output Vsync Width/Length",
                "fields": [
                    {
                        "name": "Vsync Width", 
                        "default_value": 0x06,
                        "input_type": "numeric",  # To indicate the input will be numeric (0x00 to 0xFF)
                        "min_value": 0x00,  # Minimum allowed value
                        "max_value": 0xFF,   # Maximum allowed value
                        "bits": (0, 8),
                    },
                ],
                "default": 0x06
            },
            0x71: {
                "name": "Scramble",
                "fields": [
                    {
                        "name": "Scramble", 
                        "options": ["Scramble Enable", "Scramble Disable"], 
                        "bits": (0, 1),
                        "value_map": {
                            "Scramble Enable": 0,  # Binary: 01
                            "Scramble Disable": 1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x01
            },
            0x73: {
                "name": "Output P/N swap control",
                "fields": [
                    {
                        "name": "lane0 P/N swap",
                        "options": ["Swap", "No Swap"],
                        "bits": (0, 1),  # Bit 0
                        "value_map": {
                            "Swap": 1,  # Binary: 01
                            "No Swap": 0,  # Binary: 00
                        }
                    },
                    {
                        "name": "lane1 P/N swap",
                        "options": ["Swap", "No Swap"],
                        "bits": (1, 2),  # Bit 1
                        "value_map": {
                            "Swap": 1,  # Binary: 10
                            "No Swap": 0,  # Binary: 00
                        }
                    },
                    {
                        "name": "lane2 P/N swap",
                        "options": ["Swap", "No Swap"],
                        "bits": (2, 3),  # Bit 2
                        "value_map": {
                            "Swap": 1,  # Binary: 100
                            "No Swap": 0,  # Binary: 000
                        }
                    },
                    {
                        "name": "lane3 P/N swap",
                        "options": ["Swap", "No Swap"],
                        "bits": (3, 4),  # Bit 3
                        "value_map": {
                            "Swap": 1,  # Binary: 1000
                            "No Swap": 0,  # Binary: 0000
                        }
                    },
                ],
                "default": 0x00,  # All lanes set to No Swap by default
            },
            0x74: {
                "name": "Bit depth",
                "fields": [
                    {
                        "name": "Bit depth selection", 
                        "options": ["6-bit", "8-bit"], 
                        "bits": (5, 6),
                        "value_map": {
                            "6-bit": 0,  # Binary: 01
                            "8-bit": 1,  # Binary: 00
                        }
                    },
                ],
                "default": 0x00
            },        
        }

        self.config_values = {reg: data["default"] for reg, data in self.registers.items()}
        # Set to store configuration signatures that failed during testing.
        self.failed_configs = set()

        self.create_widgets()

    def create_widgets(self):
        main_frame = tk.Frame(self.root)
        main_frame.pack(fill="both", expand=True, padx=10, pady=10)

        canvas = tk.Canvas(main_frame)
        scrollbar = tk.Scrollbar(main_frame, orient="vertical", command=canvas.yview)
        self.frame = tk.Frame(canvas)

        self.frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.create_window((0, 0), window=self.frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        for reg, data in self.registers.items():
            reg_frame = tk.LabelFrame(self.frame, text=f"Register 0x{reg:02X}: {data['name']}")
            reg_frame.pack(fill="x", padx=5, pady=5)

            row = 0
            for field in data["fields"]:
                tk.Label(reg_frame, text=field["name"]).grid(row=row, column=0, sticky="w", padx=5, pady=2)

                if field.get("value_map"):  # For dropdowns
                    var = tk.StringVar(value=self.get_current_field_value(reg, field))
                    dropdown = ttk.Combobox(reg_frame, textvariable=var, values=field["options"], state="readonly")
                    dropdown.grid(row=row, column=1, padx=5, pady=2)
                    dropdown.bind("<<ComboboxSelected>>", lambda e, r=reg, f=field, v=var: self.update_register(r, f, v))
                    field["variable"] = var  # Store the variable for syncing

                else:  # For numeric inputs
                    var = tk.IntVar(value=self.get_current_field_value(reg, field))
                    entry = tk.Entry(reg_frame, textvariable=var)
                    entry.grid(row=row, column=1, padx=5, pady=2)
                    entry.bind("<FocusOut>", lambda e, r=reg, f=field, v=var: self.update_register_numeric(r, f, v))
                    field["variable"] = var  # Store the variable for syncing

                row += 1

            value_label = tk.Label(reg_frame, text=f"Value: 0x{data['default']:02X}", width=15)
            value_label.grid(row=0, column=2, rowspan=len(data["fields"]), sticky="e")
            data["value_label"] = value_label

        tk.Button(self.root, text="Generate Config File", command=self.generate_config_file).pack(pady=10)
        tk.Button(self.root, text="Load Config File", command=self.load_config_file).pack(pady=10)
        
    def get_current_field_value(self, reg, field):
        """
        Retrieve the current value of a field based on the config value and field bits.
        """
        current_value = self.config_values[reg]
        start_bit, end_bit = field["bits"]
        mask = (1 << (end_bit - start_bit)) - 1
        field_value = (current_value >> start_bit) & mask

        # Map the value back to the corresponding option if a value_map exists
        if "value_map" in field:
            for option, mapped_value in field["value_map"].items():
                if mapped_value == field_value:
                    return option
        return field_value

    def update_register_numeric(self, reg, field, var):
        try:
            new_value = int(var.get())
        except ValueError:
            var.set(self.get_current_field_value(reg, field))
            return

        start_bit, end_bit = field["bits"]
        mask = ((1 << (end_bit - start_bit)) - 1) << start_bit
        self.config_values[reg] = (self.config_values[reg] & ~mask) | (new_value << start_bit)

        # Update the displayed register value
        self.registers[reg]["value_label"].config(text=f"Value: 0x{self.config_values[reg]:02X}")
        self.sync_ui_with_register(reg)

    def update_register(self, reg, field, var):
        selected_option = var.get()
        if "value_map" in field:
            option_value = field["value_map"][selected_option]
        else:
            option_value = field["options"].index(selected_option)

        start_bit, end_bit = field["bits"]
        mask = ((1 << (end_bit - start_bit)) - 1) << start_bit
        self.config_values[reg] = (self.config_values[reg] & ~mask) | (option_value << start_bit)

        # Update the displayed register value
        self.registers[reg]["value_label"].config(text=f"Value: 0x{self.config_values[reg]:02X}")
        self.sync_ui_with_register(reg)

    def sync_ui_with_register(self, reg):
        """
        Synchronize the UI fields with the current register value.
        """
        for field in self.registers[reg]["fields"]:
            current_value = self.get_current_field_value(reg, field)
            if isinstance(field["variable"], tk.StringVar):
                field["variable"].set(current_value)
            elif isinstance(field["variable"], tk.IntVar):
                field["variable"].set(int(current_value))

    def compute_config_signature(self):
        """
        Create a unique signature (tuple) for the current configuration.
        """
        # Sort the items to ensure consistent ordering.
        return tuple(sorted(self.config_values.items()))

    def generate_config_file(self):
        file_path = filedialog.asksaveasfilename(defaultextension=".txt",
                                                 filetypes=[("Text Files", "*.txt")],
                                                 title="Save Configuration File")
        if not file_path:
            return

        # Compute current configuration signature.
        config_signature = self.compute_config_signature()

        # Warn if this configuration has been flagged before.
        if config_signature in self.failed_configs:
            proceed = tk.messagebox.askyesno("Warning", 
                                          "This configuration has previously been flagged as failed. Do you want to proceed?")
            if not proceed:
                return

        def format_field_comment(field, value):
            """
            Generate a comment for a single field based on its value.
            """
            start_bit, end_bit = field["bits"]
            bitmask = ((1 << (end_bit - start_bit)) - 1) << start_bit
            field_value = (value & bitmask) >> start_bit

            # Handle value mappings
            if "value_map" in field:
                for option, mapped_value in field["value_map"].items():
                    if mapped_value == field_value:
                        return f"{field['name']}: {option}"
            # Handle options list
            elif "options" in field:
                if field_value < len(field["options"]):
                    return f"{field['name']}: {field['options'][field_value]}"

            # Fallback
            return f"{field['name']}: {field_value}"

        # Write the configuration file.
        with open(file_path, "w") as file:
            for reg, data in self.registers.items():
                value = self.config_values[reg]
                reg_comment = [data["name"]]  # Start with the register name

                # Add comments for each field
                for field in data["fields"]:
                    field_comment = format_field_comment(field, value)
                    if field_comment:
                        reg_comment.append(field_comment)

                # Combine comments into a single line
                comment_str = " - ".join(reg_comment)

                # Determine whether to split based on max_value
                max_value = data["fields"][0].get("max_value", 0xFF)
                if max_value > 0xFF:  # 16-bit register
                    high_byte = (value >> 8) & 0xFF
                    low_byte = value & 0xFF
                    file.write(f"{{0x{reg:02X}, 0x{high_byte:02X}}}, // {comment_str}\n")
                    file.write(f"{{0x{reg + 1:02X}, 0x{low_byte:02X}}},\n")
                else:  # 8-bit register
                    file.write(f"{{0x{reg:02X}, 0x{value:02X}}}, // {comment_str}\n")

        tk.messagebox.showinfo("Success", f"Configuration file saved to {file_path}")

        # Ask the user if the configuration worked.
        worked = tk.messagebox.askyesno("Configuration Test", "Did the configuration work correctly?")
        if not worked:
            # Flag the configuration by storing its signature.
            self.failed_configs.add(config_signature)

    def load_config_file(self):
        """Load register values from a configuration file."""
        file_path = filedialog.askopenfilename(
            defaultextension=".txt",
            filetypes=[("Text Files", "*.txt")],
            title="Load Configuration File"
        )
        if not file_path:
            return

        try:
            with open(file_path, 'r') as file:
                content = file.read()
                
            # Parse the file content
            lines = content.split('\n')
            register_values = {}
            
            for line in lines:
                if not line.strip():
                    continue
                    
                # Extract register and value
                match = re.match(r'{0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+)}', line)
                if not match:
                    continue
                    
                reg = int(match.group(1), 16)
                value = int(match.group(2), 16)
                
                # Handle 16-bit registers
                if reg in self.registers:
                    if any(field.get("max_value", 0xFF) > 0xFF for field in self.registers[reg]["fields"]):
                        # This is the high byte of a 16-bit value
                        if reg + 1 not in register_values:
                            register_values[reg] = value << 8
                        else:
                            register_values[reg] |= value
                    else:
                        # This is an 8-bit register
                        register_values[reg] = value
                elif reg - 1 in self.registers:
                    # This is the low byte of a 16-bit value
                    if reg - 1 in register_values:
                        register_values[reg - 1] |= value
                    else:
                        register_values[reg - 1] = value
            
            # Update the GUI with the loaded values
            for reg, value in register_values.items():
                if reg in self.registers:
                    self.config_values[reg] = value
                    
                    # Update all fields for this register
                    for field in self.registers[reg]["fields"]:
                        start_bit, end_bit = field["bits"]
                        mask = ((1 << (end_bit - start_bit)) - 1)
                        field_value = (value >> start_bit) & mask
                        
                        if "value_map" in field:
                            # Find the option that matches this value
                            for option, mapped_value in field["value_map"].items():
                                if mapped_value == field_value:
                                    field["variable"].set(option)
                                    break
                        else:
                            # For numeric fields
                            field["variable"].set(field_value)
                    
                    # Update the value label
                    self.registers[reg]["value_label"].config(text=f"Value: 0x{value:02X}")
            
            tk.messagebox.showinfo("Success", "Configuration loaded successfully")
            
        except Exception as e:
            tk.messagebox.showerror("Error", f"Failed to load configuration: {str(e)}")

if __name__ == "__main__":
    root = tk.Tk()
    app = NCS8801SConfigurator(root)
    root.mainloop()