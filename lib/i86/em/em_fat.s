.define .fat
.text

.fat:
.extern .trp
.extern .stop
	call    .trp
	call    .stop
	! no return
