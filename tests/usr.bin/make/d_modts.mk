
LIST= one two three
LIST+= four five six

FU_mod-ts = a / b / cool

AAA= a a a
B.aaa= Baaa

all:   mod-ts

mod-ts:
	@echo 'LIST="${LIST}"'
	@echo 'LIST:ts,="${LIST:ts,}"'
	@echo 'LIST:ts/:tu="${LIST:ts/:tu}"'
	@echo 'LIST:ts::tu="${LIST:ts::tu}"'
	@echo 'LIST:ts:tu="${LIST:ts:tu}"'
	@echo 'LIST:tu:ts/="${LIST:tu:ts/}"'
	@echo 'LIST:ts:="${LIST:ts:}"'
	@echo 'LIST:ts="${LIST:ts}"'
	@echo 'LIST:ts:S/two/2/="${LIST:ts:S/two/2/}"'
	@echo 'LIST:S/two/2/:ts="${LIST:S/two/2/:ts}"'
	@echo 'LIST:ts/:S/two/2/="${LIST:ts/:S/two/2/}"'
	@echo "Pretend the '/' in '/n' etc. below are back-slashes."
	@echo 'LIST:ts/n="${LIST:ts\n}"'
	@echo 'LIST:ts/t="${LIST:ts\t}"'
	@echo 'LIST:ts/012:tu="${LIST:ts\012:tu}"'
	@echo 'LIST:tx="${LIST:tx}"'
	@echo 'LIST:ts/x:tu="${LIST:ts\x:tu}"'
	@echo 'FU_$@="${FU_${@:ts}:ts}"'
	@echo 'FU_$@:ts:T="${FU_${@:ts}:ts:T}" == cool?'
	@echo 'B.$${AAA:ts}="${B.${AAA:ts}}" == Baaa?'
