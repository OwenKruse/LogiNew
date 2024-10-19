// controller.rs
use std::fmt::Write as FmtWrite;
use hidapi::{HidApi, HidDevice, HidError};
use std::sync::mpsc::{channel, Sender, Receiver};

pub const VID: u16 = 0x1915;
pub const PID: u16 = 0x520c;

// Commands
const REPORT_TYPE_COMMAND: u8 = 0x02;
const COMMAND_SCRIPT_MOVE_MOUSE: u8 = 0x15;
const COMMAND_SCRIPT_INJECT: u8 = 0x17;
const COMMAND_SCRIPT_INJECT_TARGET: u8 = 0x16;
const COMMAND_SCRIPT_PASSIVE_ENUM: u8 = 0x19;
const COMMAND_SCRIPT_CLEAR: u8 = 0x14;
const COMMAND_SCRIPT_PRESS: u8 = 0x12;

pub struct HIDDeviceController {
    device: Option<HidDevice>,
    tx: Sender<Vec<u8>>,
    rx: Receiver<Vec<u8>>,
}

impl HIDDeviceController {
    pub fn new() -> Result<Self, HidError> {
        let api = HidApi::new()?;
        let (tx, rx) = channel();

        let mut controller = HIDDeviceController {
            device: None,
            tx,
            rx,
        };

        controller.open_device(&api)?;

        Ok(controller)
    }


    pub fn open_device(&mut self, api: &HidApi) -> Result<(), HidError> {
        for device_info in api.device_list() {
            if device_info.vendor_id() == VID && device_info.product_id() == PID {
                match device_info.open_device(api) {
                    Ok(device) => {
                        let manufacturer = device.get_manufacturer_string().unwrap_or_else(|_| Option::from(String::from("Unknown")));
                        let product = device.get_product_string().unwrap_or_else(|_| Option::from(String::from("Unknown")));
                        println!("Device opened successfully: {:?} {:?}", manufacturer, product);
                        self.device = Some(device);
                        self.clear()?;
                        println!("Device cleared successfully");
                        return Ok(());
                    }
                    Err(e) => println!("Failed to open device: {}", e),
                }
            }
        }
        Err(HidError::HidApiError { message: "Device not found".into() })
    }
    fn build_report(&self, report_type: u8, cmd: u8, args: &[u8]) -> Vec<u8> {
        let mut report = vec![0u8; 65];  // 65 bytes for macOS (including report ID)
        report[1] = report_type;
        report[2] = cmd;
        report[3..3+args.len()].copy_from_slice(args);
        report
    }

    pub fn cmd_mouse(&self, x: i16, y: i16, left: u8, right: u8) -> Vec<u8> {
        let mut args = vec![0u8; 6];
        args[0..2].copy_from_slice(&x.to_be_bytes());
        args[2..4].copy_from_slice(&y.to_be_bytes());
        args[4] = left;
        args[5] = right;
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_MOVE_MOUSE, &args)
    }

    pub fn cmd_inject(&self) -> Vec<u8> {
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_INJECT, &[])
    }

    pub fn cmd_inject_target(&self, target: &str) -> Vec<u8> {
        let target_bytes = target.replace(":", "").chars()
            .collect::<Vec<char>>()
            .chunks(2)
            .map(|chunk| u8::from_str_radix(&chunk.iter().collect::<String>(), 16).unwrap())
            .collect::<Vec<u8>>();
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_INJECT_TARGET, &target_bytes)
    }

    pub fn cmd_passive_enum(&self, target: &str) -> Vec<u8> {
        let target_bytes = target.replace(":", "").chars()
            .collect::<Vec<char>>()
            .chunks(2)
            .map(|chunk| u8::from_str_radix(&chunk.iter().collect::<String>(), 16).unwrap())
            .collect::<Vec<u8>>();
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_PASSIVE_ENUM, &target_bytes)
    }

    pub fn cmd_clear(&self) -> Vec<u8> {
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_CLEAR, &[])
    }

    pub fn cmd_press(&self, key: u8) -> Vec<u8> {
        self.build_report(REPORT_TYPE_COMMAND, COMMAND_SCRIPT_PRESS, &[key])
    }



    pub fn write_report(&self, report: Vec<u8>) -> Result<(), HidError> {
        // Log the report in both hex and ASCII format
        let mut hex_report = String::new();
        let mut ascii_report = String::new();
        for (i, &byte) in report.iter().enumerate() {
            write!(hex_report, "{:02X} ", byte).unwrap();
            write!(ascii_report, "{}", if byte.is_ascii_graphic() { byte as char } else { '.' }).unwrap();
            if (i + 1) % 16 == 0 {
                hex_report.push('\n');
                ascii_report.push('\n');
            }
        }
        println!("Sending feature report:\nHex:\n{}\nASCII:\n{}", hex_report, ascii_report);

        // Check report length
        if report.len() > 255 {
            println!("Warning: Report length ({}) exceeds 255 bytes", report.len());
        }

        if let Some(device) = &self.device {
            // Attempt to send the feature report
            match device.write(&report) {
                Ok(_) => {
                    println!("Successfully sent feature report of {} bytes", report.len());
                    Ok(())
                },
                Err(e) => {
                    println!("Failed to send feature report: {:?}", e);
                    println!("Error details: {}", e);
                    Err(e)
                }
            }
        } else {
            println!("Error: Device not opened");
            Err(HidError::HidApiError { message: "Device not opened".into() })
        }
    }
    pub fn move_mouse(&self, x: i16, y: i16, left: u8, right: u8) -> Result<(), HidError> {
        let report = self.cmd_mouse(x, y, left, right);
        self.write_report(report)?;
        Ok(())
    }

    pub fn set_target(&self, target: &str) -> Result<(), HidError> {
        let report = self.cmd_inject_target(target);
        self.write_report(report)?;
        Ok(())
    }

    pub fn passive_enum(&self, target: &str) -> Result<(), HidError> {
        let report = self.cmd_passive_enum(target);
        self.write_report(report)?;
        Ok(())
    }

    pub fn inject(&self) -> Result<(), HidError> {
        let report = self.cmd_inject();
        self.write_report(report)?;
        Ok(())
    }

    pub fn set_inject_target(&self, target: &str) -> Result<(), HidError> {
        let report = self.cmd_inject_target(target);
        self.write_report(report)?;
        Ok(())
    }

    pub fn clear(&self) -> Result<(), HidError> {
        let report = self.cmd_clear();
        self.write_report(report)?;
        Ok(())
    }

    pub fn press_key(&self, key: u8) -> Result<(), HidError> {
        let report = self.cmd_press(key);
        self.write_report(report)?;
        Ok(())
    }
}

// Utility function to print available HID devices
pub fn print_available_devices() -> Result<(), HidError> {
    let api = HidApi::new()?;
    println!("Available HID devices:");
    for device in api.device_list() {
        println!(
            "VID: {:04x}, PID: {:04x}, Manufacturer: {}, Product: {}, Serial: {}",
            device.vendor_id(),
            device.product_id(),
            device.manufacturer_string().unwrap_or("Unknown"),
            device.product_string().unwrap_or("Unknown"),
            device.serial_number().unwrap_or("Unknown")
        );
    }
    Ok(())
}