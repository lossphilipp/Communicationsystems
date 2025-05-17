-- Define the protocol
local custom_proto = Proto("lop_esp", "LOP ESP32 Protocol")

-- Define the fields of the protocol
local f_gpio_num = ProtoField.uint8("lop_esp.gpio_num", "GPIO Number", base.DEC, { [2] = "Right", [9] = "Left" })
local f_event = ProtoField.uint8("lop_esp.event", "Event", base.DEC, { [0] = "Pressed", [1] = "Released" })
local f_timestamp = ProtoField.uint64("lop_esp.timestamp", "Timestamp", base.DEC)

-- Add the fields to the protocol
custom_proto.fields = { f_gpio_num, f_event, f_timestamp }

-- Dissector function
function custom_proto.dissector(buffer, pinfo, tree)
    -- Check if the packet is long enough
    if buffer:len() < 10 then
        return  -- Not enough data for this protocol
    end

    -- Set the protocol name in the packet list
    pinfo.cols.protocol = "LOP_ESP"

    -- Add the protocol to the dissection tree
    local subtree = tree:add(custom_proto, buffer(), "LOP ESP32 Protocol Data")

    -- Parse and add fields to the tree
    subtree:add(f_gpio_num, buffer(0, 1))  -- GPIO Number (1 byte)
    subtree:add(f_event, buffer(1, 1))     -- Event (1 byte)
    subtree:add(f_timestamp, buffer(2, 8)) -- Timestamp (8 bytes, big-endian)
end

-- Register the protocol to a specific UDP port
local udp_port = DissectorTable.get("udp.port")
udp_port:add(15651, custom_proto)  -- Replace 15651 with your actual port number