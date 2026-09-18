; Conway's Game of Life for this emulator's memory map.
; Intended for ca65-like assemblers (labels/directives may need minor adaptation).
;
; Memory map assumptions:
; - Screen RAM:  $C000 (40x25 visible)
; - Work RAM:    buffers in low RAM
; - Entry point: $D000
;
; Grid strategy:
; - Use a padded 42x27 grid (dead border) for branch-light neighbor logic.
; - Visible area maps to inner 40x25 cells.
;
; Buffers:
; CUR = $0200  ; 42*27 = 1134 bytes
; NXT = $0680  ; 1134 bytes
;
; NOTE:
; This source is intentionally explicit and readable rather than cycle-optimal.
; It is a starting point you can tune once all CPU opcodes/flags are fully validated.

        .org $D000

CUR     = $0200
NXT     = $0680
SCR     = $C000
ROWW    = 42
ROWS    = 27
COLS    = 40
VISROWS = 25

; zero page variables
curLo   = $00
curHi   = $01
nxtLo   = $02
nxtHi   = $03
topLo   = $04
topHi   = $05
midLo   = $06
midHi   = $07
botLo   = $08
botHi   = $09
sumv    = $0A
cellv   = $0B
rowv    = $0C
colv    = $0D
tmpLo   = $0E
tmpHi   = $0F

Start:
        JSR ClearBuffers
        JSR SeedGlider
MainLoop:
        JSR StepLife
        JSR SwapBuffers
        JSR RenderScreen
        JMP MainLoop

; ------------------------------------------------------------
; Clear CUR and NXT (each 1134 bytes)
; ------------------------------------------------------------
ClearBuffers:
        LDA #$00
        ; CUR clear (5 pages + 110 bytes)
        LDX #$00
CB_CurPages:
        STA CUR, X
        STA CUR+$0100, X
        STA CUR+$0200, X
        STA CUR+$0300, X
        STA CUR+$0400, X
        INX
        BNE CB_CurPages
        LDX #$00
CB_CurTail:
        CPX #$6E
        BEQ CB_Nxt
        STA CUR+$0500, X
        INX
        JMP CB_CurTail

CB_Nxt:
        LDX #$00
CB_NxtPages:
        STA NXT, X
        STA NXT+$0100, X
        STA NXT+$0200, X
        STA NXT+$0300, X
        STA NXT+$0400, X
        INX
        BNE CB_NxtPages
        LDX #$00
CB_NxtTail:
        CPX #$6E
        BEQ CB_Done
        STA NXT+$0500, X
        INX
        JMP CB_NxtTail
CB_Done:
        RTS

; ------------------------------------------------------------
; Seed a glider around visible (x=5,y=5) in CUR (padded coords x+1,y+1)
; ------------------------------------------------------------
SeedGlider:
        ; (6,5), (7,6), (5,7), (6,7), (7,7)
        LDA #$01
        STA CUR + (6 + 5*ROWW)
        STA CUR + (7 + 6*ROWW)
        STA CUR + (5 + 7*ROWW)
        STA CUR + (6 + 7*ROWW)
        STA CUR + (7 + 7*ROWW)
        RTS

; ------------------------------------------------------------
; StepLife
; for y=1..25, x=1..40:
;   sum = 8 neighbors from CUR
;   if CUR(y,x)==1: alive iff sum==2 or sum==3
;   else            alive iff sum==3
;   store into NXT(y,x)
; ------------------------------------------------------------
StepLife:
        LDA #$01
        STA rowv
SL_RowLoop:
        ; Build pointers to top/mid/bot rows and next row.
        ; base = CUR + row*42
        JSR RowPtrMid
        ; top = mid - 42
        SEC
        LDA midLo
        SBC #<ROWW
        STA topLo
        LDA midHi
        SBC #>ROWW
        STA topHi
        ; bot = mid + 42
        CLC
        LDA midLo
        ADC #<ROWW
        STA botLo
        LDA midHi
        ADC #>ROWW
        STA botHi
        ; nxt = NXT + row*42
        JSR RowPtrNxt

        LDA #$01
        STA colv
SL_ColLoop:
        ; sum = top[x-1] + top[x] + top[x+1] + mid[x-1] + mid[x+1] + bot[x-1] + bot[x] + bot[x+1]
        LDA #$00
        STA sumv

        ; top[x-1]
        LDY colv
        DEY
        LDA (topLo),Y
        CLC
        ADC sumv
        STA sumv
        ; top[x]
        LDY colv
        LDA (topLo),Y
        CLC
        ADC sumv
        STA sumv
        ; top[x+1]
        LDY colv
        INY
        LDA (topLo),Y
        CLC
        ADC sumv
        STA sumv
        ; mid[x-1]
        LDY colv
        DEY
        LDA (midLo),Y
        CLC
        ADC sumv
        STA sumv
        ; mid[x+1]
        LDY colv
        INY
        LDA (midLo),Y
        CLC
        ADC sumv
        STA sumv
        ; bot[x-1]
        LDY colv
        DEY
        LDA (botLo),Y
        CLC
        ADC sumv
        STA sumv
        ; bot[x]
        LDY colv
        LDA (botLo),Y
        CLC
        ADC sumv
        STA sumv
        ; bot[x+1]
        LDY colv
        INY
        LDA (botLo),Y
        CLC
        ADC sumv
        STA sumv

        ; cellv = mid[x]
        LDY colv
        LDA (midLo),Y
        STA cellv

        ; rule:
        ; if cellv==1 -> alive if sumv==2 or 3
        ; if cellv==0 -> alive if sumv==3
        LDA cellv
        CMP #$01
        BNE SL_DeadBranch
        LDA sumv
        CMP #$02
        BEQ SL_WriteAlive
        CMP #$03
        BEQ SL_WriteAlive
        JMP SL_WriteDead

SL_DeadBranch:
        LDA sumv
        CMP #$03
        BEQ SL_WriteAlive
        JMP SL_WriteDead

SL_WriteAlive:
        LDA #$01
        LDY colv
        STA (nxtLo),Y
        JMP SL_Advance

SL_WriteDead:
        LDA #$00
        LDY colv
        STA (nxtLo),Y

SL_Advance:
        INC colv
        LDA colv
        CMP #$29        ; 41 -> stop after x=40
        BEQ SL_ColDone
        JMP SL_ColLoop

SL_ColDone:
        INC rowv
        LDA rowv
        CMP #$1A        ; 26 -> stop after y=25
        BEQ SL_Done
        JMP SL_RowLoop
SL_Done:
        RTS

; mid = CUR + rowv*42
RowPtrMid:
        LDA #<CUR
        STA midLo
        LDA #>CUR
        STA midHi
        LDX rowv
RPM_Loop:
        CPX #$00
        BEQ RPM_Done
        CLC
        LDA midLo
        ADC #<ROWW
        STA midLo
        LDA midHi
        ADC #>ROWW
        STA midHi
        DEX
        JMP RPM_Loop
RPM_Done:
        RTS

; nxt = NXT + rowv*42
RowPtrNxt:
        LDA #<NXT
        STA nxtLo
        LDA #>NXT
        STA nxtHi
        LDX rowv
RPN_Loop:
        CPX #$00
        BEQ RPN_Done
        CLC
        LDA nxtLo
        ADC #<ROWW
        STA nxtLo
        LDA nxtHi
        ADC #>ROWW
        STA nxtHi
        DEX
        JMP RPN_Loop
RPN_Done:
        RTS

; swap CUR and NXT bases by copying NXT -> CUR and clearing NXT.
; (simple approach; can be optimized with pointer swap if preferred)
SwapBuffers:
        LDX #$00
SB_Pages:
        LDA NXT, X
        STA CUR, X
        LDA NXT+$0100, X
        STA CUR+$0100, X
        LDA NXT+$0200, X
        STA CUR+$0200, X
        LDA NXT+$0300, X
        STA CUR+$0300, X
        LDA NXT+$0400, X
        STA CUR+$0400, X
        LDA #$00
        STA NXT, X
        STA NXT+$0100, X
        STA NXT+$0200, X
        STA NXT+$0300, X
        STA NXT+$0400, X
        INX
        BNE SB_Pages

        LDX #$00
SB_Tail:
        CPX #$6E
        BEQ SB_Done
        LDA NXT+$0500, X
        STA CUR+$0500, X
        LDA #$00
        STA NXT+$0500, X
        INX
        JMP SB_Tail
SB_Done:
        RTS

; Render CUR inner 40x25 to SCR:
; 0 -> space ($20), 1 -> '*' ($2A)
RenderScreen:
        LDA #$00
        STA rowv
RS_RowLoop:
        ; mid = CUR + (row+1)*42
        LDA rowv
        CLC
        ADC #$01
        STA tmpLo
        LDA #<CUR
        STA midLo
        LDA #>CUR
        STA midHi
        LDX tmpLo
RS_RowPtr:
        CPX #$00
        BEQ RS_RowPtrDone
        CLC
        LDA midLo
        ADC #<ROWW
        STA midLo
        LDA midHi
        ADC #>ROWW
        STA midHi
        DEX
        JMP RS_RowPtr
RS_RowPtrDone:
        ; build screen pointer: SCR + row*40
        LDA #<SCR
        STA tmpLo
        LDA #>SCR
        STA tmpHi
        LDX rowv
RS_ScrPtr:
        CPX #$00
        BEQ RS_ScrDone
        CLC
        LDA tmpLo
        ADC #$28        ; +40
        STA tmpLo
        LDA tmpHi
        ADC #$00
        STA tmpHi
        DEX
        JMP RS_ScrPtr
RS_ScrDone:
        ; columns 0..39, source x=1..40
        LDY #$00
RS_ColLoop:
        LDA (midLo),Y
        ; shift source one to the right (x+1)
        INY
        LDA (midLo),Y
        CMP #$00
        BEQ RS_WriteSpace
        LDA #$2A
        JMP RS_Write
RS_WriteSpace:
        LDA #$20
RS_Write:
        DEY
        STA (tmpLo),Y
        INY
        CPY #$28
        BNE RS_ColLoop

        INC rowv
        LDA rowv
        CMP #$19
        BNE RS_RowLoop
        RTS

; set reset vector used by this emulator at $FFFE
        .org $FFFE
        .word Start
