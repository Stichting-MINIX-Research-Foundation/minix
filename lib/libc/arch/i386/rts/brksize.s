.sect .text; .sect .rom; .sect .data; .sect .bss
.define __brksize
.sect .data
.extern endbss, __brksize
__brksize: .data4 endbss
