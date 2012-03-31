import magic

ms = magic.open(magic.MAGIC_NONE)
ms.load()
type =  ms.file("/path/to/some/file")
print type

f = file("/path/to/some/file", "r")
buffer = f.read(4096)
f.close()

type = ms.buffer(buffer)
print type

ms.close()

