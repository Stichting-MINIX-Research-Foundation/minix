#!/usr/bin/lua
--
-- weatherstation.lua - a simple web server intended to be run from inetd to
-- expose sensors from the BeagleBone Weather cape and serve the web app.
--

-- List of files shared by this service.
distfiles={}
distfiles["/index.html"] = true
distfiles["/style.css"] = true
distfiles["/jquery.js"] = true
distfiles["/processing.js"] = true
distfiles["/spin.js"] = true
distfiles["/weatherstation.js"] = true

-- Base path for distfiles
prefix="/usr/share/beaglebone/weather"

-- Check that the filename is part of this demo.
function filename_is_valid(filename)
	return distfiles[filename] ~= nil
end

-- Check if the string 's' starts with 'starting'
function starts_with(s, starting)
	return string.sub(s, 1, string.len(starting)) == starting
end

-- Check if the string 's' ends with 'ending'
function ends_with(s, ending)
	return string.sub(s, -string.len(ending)) == ending
end

-- Return the content type of the file based only on the file extension.
function get_content_type(filename)

	if ends_with(filename, ".js") then
		return "application/javascript"
	elseif ends_with(filename, ".css") then
		return "text/css"
	elseif ends_with(filename, ".html") then
		return "text/html"
	elseif ends_with(filename, ".json") then
		return "application/json"
	else
		return "text/plain"
	end
end

-- Reads from STDIN until an empty or nil line is received.
-- Returns the first line of the request.
function read_request()

	local i = 0
	local lines = {}

	repeat
		local line = io.read("*line")
		if line == nil or line == "\r" then
			break
		end
		lines[i] = line
		i = i + 1
	until false

	return lines[0]
end

-- Return the entire contents of a file
function read_file(filename)
	local f = io.open(filename, "rb")

	if f == nil then
		return nil
	end
	local content = f:read("*all")
	f:close()

	return content
end

-- Extract the value of the LABEL:VALUE pairs read from the device files.
function extract_value(data, pat)

	local x = string.match(data, pat)

	if x == nil then
		return "0"
	end
	return x
end

-- Read the sensor values and generate json output
function generate_json()
	local json = ""

	local tsl2550 = read_file("/dev/tsl2550b3s39")
	if tsl2550 == nil then
		return nil
	end
	local illuminance = extract_value(tsl2550, "ILLUMINANCE: (%d+)")

	local sht21 = read_file("/dev/sht21b3s40")
	if sht21 == nil then
		return nil
	end
	local temperature = extract_value(sht21, "TEMPERATURE: (%d+.%d+)")
	local humidity = extract_value(sht21, "HUMIDITY: (%d+.%d+)")

	local bmp085 = read_file("/dev/bmp085b3s77")
	if bmp085 == nil then
		return nil
	end
	local pressure = extract_value(bmp085, "PRESSURE: (%d+)")

	json = json .. "{\n"
	json = json .. "\"temperature\": " .. temperature .. ",\n"
	json = json .. "\"humidity\": " .. humidity .. ",\n"
	json = json .. "\"illuminance\": " .. illuminance .. ",\n"
	json = json .. "\"pressure\": " .. pressure .. "\n"
	json = json .. "}\n"

	return json
end

function handle_request(req)

	if req == nil then
		response("404 Not Found", "text/plain", "Unknown Request")
		return
	end

	-- Parse filename out of HTTP request
	local filename = (string.match(req, " %S+ "):gsub("^%s+", ""):gsub("%s+$", ""))
	if filename == "" or filename == "/" then
		filename = "/index.html"
	end

	-- Check if the filename is known (i.e. it's a file in the web app)
	if filename_is_valid(filename) then
		local contents = read_file(prefix .. filename)

		if contents ~= nil then
			response("200 OK", get_content_type(filename), contents)
		else
			response("404 Not Found", "text/plain", "Failed to load known file")
		end
	-- Else maybe the user is requesting the dynamic json data
	elseif starts_with(filename, "/weather.json") then
		local json = generate_json()
		if json == nil then
			response("500 Internal Server Error", "text/plain", "Could not get sensor values")
		else
			response("200 OK", get_content_type("/weather.json"), json)
		end
	-- Else, the user is requesting something not part of the app
	else
		response("403 Forbidden", "text/plain", "File not allowed.")
	end

	return
end

-- Send the response to the HTTP request.
function response(err_code, content_type, content)
	io.write("HTTP/1.1 " .. err_code .. "\r\n")
	io.write("Content-Type: " .. content_type .. "\r\n")
	io.write("\r\n")
	io.write(content)
	io.write("\r\n")
end

-- Read the request and then handle it.
handle_request(read_request())

