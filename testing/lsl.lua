-- Wireshark dissector for labstreamerlayer stream discovery packets
-- Place this file in $WIRESHARK_CONFIG_DIR/plugins/lsl.lua
-- (C) 2020 Tristan Stenner
do
	local lsl_proto = Proto("lsldiscovery", "Labstreaminglayer Discovery")
	-- source fields
	local udp_src_f = Field.new("udp.srcport")
	local udp_dst_f = Field.new("udp.dstport")
	-- fields to be shown in the packet analysis pane
	local cmd_F = ProtoField.string("lsl.cmd", "Command")
	local dst_F = ProtoField.string("lsl.dst", "Destination")
	local query_F = ProtoField.string("lsl.query", "QueryID", "QueryID")
	local content_F = ProtoField.string("lsl.content", "Content", "Content")
	local streaminfo_F = ProtoField.string("lsl.streaminfo", "Streaminfo", "Streaminfo")
	local ansport_F = ProtoField.string('lsl.answerport', 'Answer port', 'Answer port')

	lsl_proto.fields = {cmd_F, dst_F, query_F, ansport_F, streaminfo_F, content_F}

	function lsl_proto.dissector(buffer, pinfo, tree)
		if udp_dst_f().value == 16571 then
			if buffer(0, 4):string()~='LSL:' then
				return
			end
			local subtree = tree:add(lsl_proto, buffer)

			local data = buffer():string()
			local newlinepos = data:find('\r\n')
			if newlinepos == nil then
				return
			end
			subtree:add(cmd_F, buffer(0, newlinepos-1))
			buffer = buffer(newlinepos + 1)
			newlinepos = buffer():string():find('\r\n', newlinepos+1, true)
			if newlinepos == nil then
				return
			end
			subtree:add(content_F, buffer(0, newlinepos-1))
			buffer = buffer(newlinepos + 1)
			local wspos = buffer():string():find(' ')
			subtree:add(ansport_F, buffer(0, wspos-1))
			subtree:add(query_F, buffer(wspos))
		elseif udp_src_f().value == 16571 then
			local subtree = tree:add(lsl_proto, buffer)
			subtree:add(cmd_F, 'shortinfo reply')

			local data = buffer():string()
			local newlinepos = data:find('\r\n')
			if newlinepos == nil then
				return
			end
			subtree:add(query_F, buffer(0, newlinepos-1))
			buffer = buffer(newlinepos + 1)
			subtree:add(streaminfo_F, buffer(newlinepos+1))
		end
	end
	DissectorTable.get('udp.port'):add(16571, lsl_proto)
end
