.define .unknown
.text
.extern .fat
EILLINS = 18

.unknown:
	mov  ax,#EILLINS
	push ax
	jmp  .fat
