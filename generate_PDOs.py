import re

def parse_cansend_line(line, command_index):
    match = re.match(r'cansend can0 (\w+)#(\w{8})(\w{8})?', line)
    if not match:
        return None
    
    can_id = match.group(1)
    data_high = match.group(2)
    data_low = match.group(3) if match.group(3) else "00000000"
    
    return f"setp ds301.can_command_{command_index}_id 0x{can_id}\n" \
           f"setp ds301.can_command_{command_index}_data_high 0x{data_high}\n" \
           f"setp ds301.can_command_{command_index}_data_low  0x{data_low}\n"

def convert_scripts(input_files, output_file):
    output_lines = []
    command_index = 0
    
    for input_file in input_files:
        with open(input_file, 'r') as file:
            lines = file.readlines()
        
        for line in lines:
            line = line.strip()
            if line.startswith("cansend can0"):
                parsed_command = parse_cansend_line(line, command_index)
                if parsed_command:
                    output_lines.append(parsed_command)
                    command_index += 1
    
    with open(output_file, 'w') as file:
        file.writelines(output_lines)

if __name__ == "__main__":
    input_files = ["txPDO1.sh", "txPDO2.sh"]  # List of input files
    output_file = "PDOs.hal"  # Output HAL file
    convert_scripts(input_files, output_file)
    #print(f"Output written to {output_file}")
