.define __brksize
.data
.extern endbss, __brksize
__brksize: .data2 endbss
