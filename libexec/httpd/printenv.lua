-- $NetBSD: printenv.lua,v 1.2 2014/01/02 08:21:38 mrg Exp $

-- this small Lua script demonstrates the use of Lua in (bozo)httpd
-- it will simply output the "environment"

-- Keep in mind that bozohttpd forks for each request when started in
-- daemon mode, you can set global veriables here, but they will have
-- the same value on each invocation.  You can not keep state between
-- two calls.

local httpd = require 'httpd'

function printenv(env, headers, query)

	-- we get the "environment" in the env table, the values are more
	-- or less the same as the variable for a CGI program

	if count == nil then
		count = 1
	end

	-- output a header
	print([[
		<html>
			<head>
				<title>Bozotic Lua Environment</title>
			</head>
			<body>
				<h1>Bozotic Lua Environment</h1>
	]])

	print('module version: ' .. httpd._VERSION .. '<br>')

	print('<h2>Server Environment</h2>')
	-- print the list of "environment" variables
	for k, v in pairs(env) do
		print(k .. '=' .. v .. '<br/>')
	end

	print('<h2>Request Headers</h2>')
	for k, v in pairs(headers) do
		print(k .. '=' .. v .. '<br/>')
	end

	if query ~= nil then
		print('<h2>Query Variables</h2>')
		for k, v in pairs(query) do
			print(k .. '=' .. v .. '<br/>')
		end
	end

	print('<h2>Form Test</h2>')

	print([[
	<form method="POST" action="/rest/form?sender=me">
	<input type="text" name="a_value">
	<input type="submit">
	</form>
	]])
	-- output a footer
	print([[
		</body>
	</html>
	]])
end

function form(env, header, query)
	if query ~= nil then
		print('<h2>Form Variables</h2>')

		if env.CONTENT_TYPE ~= nil then
			print('Content-type: ' .. env.CONTENT_TYPE .. '<br>')
		end

		for k, v in pairs(query) do
			print(k .. '=' .. v .. '<br/>')
		end
	else
		print('No values')
	end
end

-- register this handler for http://<hostname>/<prefix>/printenv
httpd.register_handler('printenv', printenv)
httpd.register_handler('form', form)
