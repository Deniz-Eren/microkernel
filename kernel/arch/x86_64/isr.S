/* IDT trap stubs. Frame layout must match kernel/include/kernel/trapframe.h:
 *   ... r15 r14 ... rax | vec err | rip cs rflags rsp ss   (low -> high)
 * Vectors without an error code get a dummy 0 pushed in its slot. */

.section .text

.macro PUSHALL
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
.endm

.macro POPALL
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
.endm

/* %1 = vector, %2 = 1 if the CPU pushes an error code */
.macro ISR_STUB vec errflag
    .globl isr_stub_\vec
isr_stub_\vec:
    .if \errflag == 0
        pushq $0
    .endif
    pushq $\vec
    PUSHALL
    movq %rsp, %rdi
    callq interrupt_handler
    POPALL
    .if \errflag == 0
        addq $16, %rsp
    .else
        addq $8, %rsp
    .endif
    iretq
.endm

ISR_STUB 0 0
ISR_STUB 1 0
ISR_STUB 2 0
ISR_STUB 3 0
ISR_STUB 4 0
ISR_STUB 5 0
ISR_STUB 6 0
ISR_STUB 7 0
ISR_STUB 8 1
ISR_STUB 9 0
ISR_STUB 10 1
ISR_STUB 11 1
ISR_STUB 12 1
ISR_STUB 13 1
ISR_STUB 14 1
ISR_STUB 15 0
ISR_STUB 16 0
ISR_STUB 17 1
ISR_STUB 18 0
ISR_STUB 19 0
ISR_STUB 20 0
ISR_STUB 21 1
ISR_STUB 22 0
ISR_STUB 23 0
ISR_STUB 24 0
ISR_STUB 25 0
ISR_STUB 26 0
ISR_STUB 27 0
ISR_STUB 28 0
ISR_STUB 29 1
ISR_STUB 30 1
ISR_STUB 31 0

ISR_STUB 32 0
ISR_STUB 33 0
ISR_STUB 34 0
ISR_STUB 35 0
ISR_STUB 36 0
ISR_STUB 37 0
ISR_STUB 38 0
ISR_STUB 39 0
ISR_STUB 40 0
ISR_STUB 41 0
ISR_STUB 42 0
ISR_STUB 43 0
ISR_STUB 44 0
ISR_STUB 45 0
ISR_STUB 46 0
ISR_STUB 47 0
ISR_STUB 48 0
ISR_STUB 49 0
ISR_STUB 50 0
ISR_STUB 51 0
ISR_STUB 52 0
ISR_STUB 53 0
ISR_STUB 54 0
ISR_STUB 55 0
ISR_STUB 56 0
ISR_STUB 57 0
ISR_STUB 58 0
ISR_STUB 59 0
ISR_STUB 60 0
ISR_STUB 61 0
ISR_STUB 62 0
ISR_STUB 63 0
ISR_STUB 64 0
ISR_STUB 65 0
ISR_STUB 66 0
ISR_STUB 67 0
ISR_STUB 68 0
ISR_STUB 69 0
ISR_STUB 70 0
ISR_STUB 71 0
ISR_STUB 72 0
ISR_STUB 73 0
ISR_STUB 74 0
ISR_STUB 75 0
ISR_STUB 76 0
ISR_STUB 77 0
ISR_STUB 78 0
ISR_STUB 79 0
ISR_STUB 80 0
ISR_STUB 81 0
ISR_STUB 82 0
ISR_STUB 83 0
ISR_STUB 84 0
ISR_STUB 85 0
ISR_STUB 86 0
ISR_STUB 87 0
ISR_STUB 88 0
ISR_STUB 89 0
ISR_STUB 90 0
ISR_STUB 91 0
ISR_STUB 92 0
ISR_STUB 93 0
ISR_STUB 94 0
ISR_STUB 95 0
ISR_STUB 96 0
ISR_STUB 97 0
ISR_STUB 98 0
ISR_STUB 99 0
ISR_STUB 100 0
ISR_STUB 101 0
ISR_STUB 102 0
ISR_STUB 103 0
ISR_STUB 104 0
ISR_STUB 105 0
ISR_STUB 106 0
ISR_STUB 107 0
ISR_STUB 108 0
ISR_STUB 109 0
ISR_STUB 110 0
ISR_STUB 111 0
ISR_STUB 112 0
ISR_STUB 113 0
ISR_STUB 114 0
ISR_STUB 115 0
ISR_STUB 116 0
ISR_STUB 117 0
ISR_STUB 118 0
ISR_STUB 119 0
ISR_STUB 120 0
ISR_STUB 121 0
ISR_STUB 122 0
ISR_STUB 123 0
ISR_STUB 124 0
ISR_STUB 125 0
ISR_STUB 126 0
ISR_STUB 127 0
ISR_STUB 128 0
ISR_STUB 129 0
ISR_STUB 130 0
ISR_STUB 131 0
ISR_STUB 132 0
ISR_STUB 133 0
ISR_STUB 134 0
ISR_STUB 135 0
ISR_STUB 136 0
ISR_STUB 137 0
ISR_STUB 138 0
ISR_STUB 139 0
ISR_STUB 140 0
ISR_STUB 141 0
ISR_STUB 142 0
ISR_STUB 143 0
ISR_STUB 144 0
ISR_STUB 145 0
ISR_STUB 146 0
ISR_STUB 147 0
ISR_STUB 148 0
ISR_STUB 149 0
ISR_STUB 150 0
ISR_STUB 151 0
ISR_STUB 152 0
ISR_STUB 153 0
ISR_STUB 154 0
ISR_STUB 155 0
ISR_STUB 156 0
ISR_STUB 157 0
ISR_STUB 158 0
ISR_STUB 159 0
ISR_STUB 160 0
ISR_STUB 161 0
ISR_STUB 162 0
ISR_STUB 163 0
ISR_STUB 164 0
ISR_STUB 165 0
ISR_STUB 166 0
ISR_STUB 167 0
ISR_STUB 168 0
ISR_STUB 169 0
ISR_STUB 170 0
ISR_STUB 171 0
ISR_STUB 172 0
ISR_STUB 173 0
ISR_STUB 174 0
ISR_STUB 175 0
ISR_STUB 176 0
ISR_STUB 177 0
ISR_STUB 178 0
ISR_STUB 179 0
ISR_STUB 180 0
ISR_STUB 181 0
ISR_STUB 182 0
ISR_STUB 183 0
ISR_STUB 184 0
ISR_STUB 185 0
ISR_STUB 186 0
ISR_STUB 187 0
ISR_STUB 188 0
ISR_STUB 189 0
ISR_STUB 190 0
ISR_STUB 191 0
ISR_STUB 192 0
ISR_STUB 193 0
ISR_STUB 194 0
ISR_STUB 195 0
ISR_STUB 196 0
ISR_STUB 197 0
ISR_STUB 198 0
ISR_STUB 199 0
ISR_STUB 200 0
ISR_STUB 201 0
ISR_STUB 202 0
ISR_STUB 203 0
ISR_STUB 204 0
ISR_STUB 205 0
ISR_STUB 206 0
ISR_STUB 207 0
ISR_STUB 208 0
ISR_STUB 209 0
ISR_STUB 210 0
ISR_STUB 211 0
ISR_STUB 212 0
ISR_STUB 213 0
ISR_STUB 214 0
ISR_STUB 215 0
ISR_STUB 216 0
ISR_STUB 217 0
ISR_STUB 218 0
ISR_STUB 219 0
ISR_STUB 220 0
ISR_STUB 221 0
ISR_STUB 222 0
ISR_STUB 223 0
ISR_STUB 224 0
ISR_STUB 225 0
ISR_STUB 226 0
ISR_STUB 227 0
ISR_STUB 228 0
ISR_STUB 229 0
ISR_STUB 230 0
ISR_STUB 231 0
ISR_STUB 232 0
ISR_STUB 233 0
ISR_STUB 234 0
ISR_STUB 235 0
ISR_STUB 236 0
ISR_STUB 237 0
ISR_STUB 238 0
ISR_STUB 239 0
ISR_STUB 240 0
ISR_STUB 241 0
ISR_STUB 242 0
ISR_STUB 243 0
ISR_STUB 244 0
ISR_STUB 245 0
ISR_STUB 246 0
ISR_STUB 247 0
ISR_STUB 248 0
ISR_STUB 249 0
ISR_STUB 250 0
ISR_STUB 251 0
ISR_STUB 252 0
ISR_STUB 253 0
ISR_STUB 254 0
ISR_STUB 255 0

.section .rodata
.globl isr_stub_table
isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    .quad isr_stub_32
    .quad isr_stub_33
    .quad isr_stub_34
    .quad isr_stub_35
    .quad isr_stub_36
    .quad isr_stub_37
    .quad isr_stub_38
    .quad isr_stub_39
    .quad isr_stub_40
    .quad isr_stub_41
    .quad isr_stub_42
    .quad isr_stub_43
    .quad isr_stub_44
    .quad isr_stub_45
    .quad isr_stub_46
    .quad isr_stub_47
    .quad isr_stub_48
    .quad isr_stub_49
    .quad isr_stub_50
    .quad isr_stub_51
    .quad isr_stub_52
    .quad isr_stub_53
    .quad isr_stub_54
    .quad isr_stub_55
    .quad isr_stub_56
    .quad isr_stub_57
    .quad isr_stub_58
    .quad isr_stub_59
    .quad isr_stub_60
    .quad isr_stub_61
    .quad isr_stub_62
    .quad isr_stub_63
    .quad isr_stub_64
    .quad isr_stub_65
    .quad isr_stub_66
    .quad isr_stub_67
    .quad isr_stub_68
    .quad isr_stub_69
    .quad isr_stub_70
    .quad isr_stub_71
    .quad isr_stub_72
    .quad isr_stub_73
    .quad isr_stub_74
    .quad isr_stub_75
    .quad isr_stub_76
    .quad isr_stub_77
    .quad isr_stub_78
    .quad isr_stub_79
    .quad isr_stub_80
    .quad isr_stub_81
    .quad isr_stub_82
    .quad isr_stub_83
    .quad isr_stub_84
    .quad isr_stub_85
    .quad isr_stub_86
    .quad isr_stub_87
    .quad isr_stub_88
    .quad isr_stub_89
    .quad isr_stub_90
    .quad isr_stub_91
    .quad isr_stub_92
    .quad isr_stub_93
    .quad isr_stub_94
    .quad isr_stub_95
    .quad isr_stub_96
    .quad isr_stub_97
    .quad isr_stub_98
    .quad isr_stub_99
    .quad isr_stub_100
    .quad isr_stub_101
    .quad isr_stub_102
    .quad isr_stub_103
    .quad isr_stub_104
    .quad isr_stub_105
    .quad isr_stub_106
    .quad isr_stub_107
    .quad isr_stub_108
    .quad isr_stub_109
    .quad isr_stub_110
    .quad isr_stub_111
    .quad isr_stub_112
    .quad isr_stub_113
    .quad isr_stub_114
    .quad isr_stub_115
    .quad isr_stub_116
    .quad isr_stub_117
    .quad isr_stub_118
    .quad isr_stub_119
    .quad isr_stub_120
    .quad isr_stub_121
    .quad isr_stub_122
    .quad isr_stub_123
    .quad isr_stub_124
    .quad isr_stub_125
    .quad isr_stub_126
    .quad isr_stub_127
    .quad isr_stub_128
    .quad isr_stub_129
    .quad isr_stub_130
    .quad isr_stub_131
    .quad isr_stub_132
    .quad isr_stub_133
    .quad isr_stub_134
    .quad isr_stub_135
    .quad isr_stub_136
    .quad isr_stub_137
    .quad isr_stub_138
    .quad isr_stub_139
    .quad isr_stub_140
    .quad isr_stub_141
    .quad isr_stub_142
    .quad isr_stub_143
    .quad isr_stub_144
    .quad isr_stub_145
    .quad isr_stub_146
    .quad isr_stub_147
    .quad isr_stub_148
    .quad isr_stub_149
    .quad isr_stub_150
    .quad isr_stub_151
    .quad isr_stub_152
    .quad isr_stub_153
    .quad isr_stub_154
    .quad isr_stub_155
    .quad isr_stub_156
    .quad isr_stub_157
    .quad isr_stub_158
    .quad isr_stub_159
    .quad isr_stub_160
    .quad isr_stub_161
    .quad isr_stub_162
    .quad isr_stub_163
    .quad isr_stub_164
    .quad isr_stub_165
    .quad isr_stub_166
    .quad isr_stub_167
    .quad isr_stub_168
    .quad isr_stub_169
    .quad isr_stub_170
    .quad isr_stub_171
    .quad isr_stub_172
    .quad isr_stub_173
    .quad isr_stub_174
    .quad isr_stub_175
    .quad isr_stub_176
    .quad isr_stub_177
    .quad isr_stub_178
    .quad isr_stub_179
    .quad isr_stub_180
    .quad isr_stub_181
    .quad isr_stub_182
    .quad isr_stub_183
    .quad isr_stub_184
    .quad isr_stub_185
    .quad isr_stub_186
    .quad isr_stub_187
    .quad isr_stub_188
    .quad isr_stub_189
    .quad isr_stub_190
    .quad isr_stub_191
    .quad isr_stub_192
    .quad isr_stub_193
    .quad isr_stub_194
    .quad isr_stub_195
    .quad isr_stub_196
    .quad isr_stub_197
    .quad isr_stub_198
    .quad isr_stub_199
    .quad isr_stub_200
    .quad isr_stub_201
    .quad isr_stub_202
    .quad isr_stub_203
    .quad isr_stub_204
    .quad isr_stub_205
    .quad isr_stub_206
    .quad isr_stub_207
    .quad isr_stub_208
    .quad isr_stub_209
    .quad isr_stub_210
    .quad isr_stub_211
    .quad isr_stub_212
    .quad isr_stub_213
    .quad isr_stub_214
    .quad isr_stub_215
    .quad isr_stub_216
    .quad isr_stub_217
    .quad isr_stub_218
    .quad isr_stub_219
    .quad isr_stub_220
    .quad isr_stub_221
    .quad isr_stub_222
    .quad isr_stub_223
    .quad isr_stub_224
    .quad isr_stub_225
    .quad isr_stub_226
    .quad isr_stub_227
    .quad isr_stub_228
    .quad isr_stub_229
    .quad isr_stub_230
    .quad isr_stub_231
    .quad isr_stub_232
    .quad isr_stub_233
    .quad isr_stub_234
    .quad isr_stub_235
    .quad isr_stub_236
    .quad isr_stub_237
    .quad isr_stub_238
    .quad isr_stub_239
    .quad isr_stub_240
    .quad isr_stub_241
    .quad isr_stub_242
    .quad isr_stub_243
    .quad isr_stub_244
    .quad isr_stub_245
    .quad isr_stub_246
    .quad isr_stub_247
    .quad isr_stub_248
    .quad isr_stub_249
    .quad isr_stub_250
    .quad isr_stub_251
    .quad isr_stub_252
    .quad isr_stub_253
    .quad isr_stub_254
    .quad isr_stub_255
