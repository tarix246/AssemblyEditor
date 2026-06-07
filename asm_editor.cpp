/*
  To all those that stumble upon this, know that this was 99% vibe-coded.
  I have no idea how this works, but I am glad it does.
  To make the matters even worse, I don't know who will ever use this code.
  --------------------------------------------------------------------
  To build it go to the terminal in the folder where this code it and type:
  
  g++ asm_editor.cpp -o AssemblyEditor.exe -std=c++20 -lgdi32 -luser32 -lkernel32 -lcomctl32 -lcomdlg32 -lshell32 -municode -mwindows

  and to run it:

  .\AssemblyEditor
  
  That should work if you have g++ installed. I have it installed through msys2, so try that.
  --------------------------------------------------------------------
  You have full right to edit this code and distribute it, as it isn't my code afterall.
  I tried to make it using ChatGPT, but it failed miserably.
  Claude is much better for coding, trust me.
*/

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <richedit.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <cstdint>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// ---------------------------------------------------------------------------
// Color palette  (dark background theme)
// ---------------------------------------------------------------------------
namespace Col {
    constexpr COLORREF BG           = 0x001E1E1E;
    constexpr COLORREF FG           = 0x00D4D4D4; // default text
    constexpr COLORREF MOV_LEA      = 0x0069DCFF; // cyan-ish
    constexpr COLORREF CALL_RET     = 0x00FF7B72; // salmon/red
    constexpr COLORREF JUMP         = 0x00FFB86C; // orange
    constexpr COLORREF STACK        = 0x00C9A0DC; // lavender
    constexpr COLORREF NOP          = 0x00555555; // muted grey
    constexpr COLORREF RIP          = 0x00888866; // muted yellow-grey
    constexpr COLORREF REGISTER     = 0x004EC9B0; // teal
    constexpr COLORREF SECTION      = 0x00D7BA7D; // gold  (bold)
    constexpr COLORREF CONSTANT     = 0x00B5CEA8; // soft green
    constexpr COLORREF LABEL        = 0x00DCDCAA; // yellow (function names)
    constexpr COLORREF COMMENT      = 0x006A9955; // green
    constexpr COLORREF STRING       = 0x00CE9178; // rust/orange
    constexpr COLORREF DIRECTIVE    = 0x00C586C0; // purple
    constexpr COLORREF BRACKET      = 0x00FFD700; // gold/yellow for [ ]
    constexpr COLORREF LINE_NUM_BG  = 0x002A2A2A;
    constexpr COLORREF LINE_NUM_FG  = 0x00858585;
    constexpr COLORREF CARET        = 0x00AEAFAD;
    constexpr COLORREF SEL_BG       = 0x00264F78;
    constexpr COLORREF CURRENT_LINE = 0x00282828;
}

// ---------------------------------------------------------------------------
// Token types
// ---------------------------------------------------------------------------
enum class TT : uint8_t {
    DEFAULT,
    MOV_LEA,
    CALL_RET,
    JUMP,
    STACK,
    NOP,
    REGISTER,
    RIP,
    SECTION,
    CONSTANT,
    LABEL,
    COMMENT,
    STRING,
    DIRECTIVE,
    BRACKET,
};

static COLORREF tokenColor(TT t) {
    switch (t) {
        case TT::MOV_LEA:   return Col::MOV_LEA;
        case TT::CALL_RET:  return Col::CALL_RET;
        case TT::JUMP:      return Col::JUMP;
        case TT::STACK:     return Col::STACK;
        case TT::NOP:       return Col::NOP;
        case TT::REGISTER:  return Col::REGISTER;
        case TT::RIP:       return Col::RIP;
        case TT::SECTION:   return Col::SECTION;
        case TT::CONSTANT:  return Col::CONSTANT;
        case TT::LABEL:     return Col::LABEL;
        case TT::COMMENT:   return Col::COMMENT;
        case TT::STRING:    return Col::STRING;
        case TT::DIRECTIVE: return Col::DIRECTIVE;
        case TT::BRACKET:   return Col::BRACKET;
        default:            return Col::FG;
    }
}

static bool tokenBold(TT t) {
    return t == TT::SECTION;
}

// ---------------------------------------------------------------------------
// Keyword tables  (lower-case for comparison)
// ---------------------------------------------------------------------------
static const char* const MOV_LEA_KW[] = {
    "mov","movb","movw","movl","movq","movs","movz","movsx","movzx","movsb","movsd","movsq","movss",
    "lea","leab","leaw","leal","leaq",
    "movabs","movbe","movd","movdqa","movdqu","movaps","movups","movhlps","movlhps",nullptr
};
static const char* const CALL_RET_KW[] = {
    "call","callq","callw","calll","ret","retq","retw","retl","retn","retf","iret","iretq","iretd",
    "syscall","sysret","sysenter","sysexit",nullptr
};
static const char* const JUMP_KW[] = {
    "jmp","jmpq","jmpw","jmpl",
    "je","jne","jz","jnz","jg","jge","jl","jle","ja","jae","jb","jbe",
    "js","jns","jo","jno","jp","jnp","jpe","jpo",
    "jcxz","jecxz","jrcxz","loop","loope","loopne","loopz","loopnz",nullptr
};
static const char* const STACK_KW[] = {
    "push","pushq","pushw","pushl","pushfd","pushfq","pusha","pushad",
    "pop","popq","popw","popl","popfd","popfq","popa","popad",
    "enter","leave",nullptr
};
static const char* const NOP_KW[] = {
    "nop","nopl","nopw",nullptr
};
// General x86-64 instructions (colored same as mov/lea)
static const char* const INSTR_KW[] = {
    // Arithmetic
    "add","addb","addw","addl","addq","adc","adcb","adcw","adcl","adcq",
    "sub","subb","subw","subl","subq","sbb","sbbb","sbbw","sbbl","sbbq",
    "mul","mulb","mulw","mull","mulq","imul","imulb","imulw","imull","imulq",
    "div","divb","divw","divl","divq","idiv","idivb","idivw","idivl","idivq",
    "inc","incb","incw","incl","incq","dec","decb","decw","decl","decq",
    "neg","negb","negw","negl","negq",
    "aaa","aas","aam","aad","daa","das",
    "cbw","cwd","cwde","cdq","cdqe","cqo",
    // Logic / bitwise
    "and","andb","andw","andl","andq",
    "or","orb","orw","orl","orq",
    "xor","xorb","xorw","xorl","xorq",
    "not","notb","notw","notl","notq",
    // Shifts / rotates
    "shl","shlb","shlw","shll","shlq","sal","salb","salw","sall","salq",
    "shr","shrb","shrw","shrl","shrq","sar","sarb","sarw","sarl","sarq",
    "rol","rolb","rolw","roll","rolq","ror","rorb","rorw","rorl","rorq",
    "rcl","rclb","rclw","rcll","rclq","rcr","rcrb","rcrw","rcrl","rcrq",
    "shld","shrd",
    // Compare / test
    "cmp","cmpb","cmpw","cmpl","cmpq",
    "test","testb","testw","testl","testq",
    // Set byte on condition
    "sete","setne","setg","setge","setl","setle","seta","setae","setb","setbe",
    "sets","setns","seto","setno","setp","setnp","setz","setnz",
    // Conditional move
    "cmove","cmovne","cmovg","cmovge","cmovl","cmovle",
    "cmova","cmovae","cmovb","cmovbe","cmovs","cmovns","cmovo","cmovno",
    "cmovz","cmovnz","cmovp","cmovnp",
    // Bit manipulation (BMI/BMI2)
    "bsf","bsr","bt","btc","btr","bts","bswap",
    "andn","blsi","blsmsk","blsr","tzcnt","lzcnt","popcnt",
    "bzhi","mulx","pdep","pext","rorx","sarx","shlx","shrx",
    // String ops
    "cmpsb","cmpsw","cmpsl","cmpsq","cmpsd",
    "scasb","scasw","scasl","scasq","scasd",
    "stosb","stosw","stosl","stosq","stosd",
    "lodsb","lodsw","lodsl","lodsq","lodsd",
    "insb","insw","insl","outsb","outsw","outsl",
    "rep","repe","repne","repz","repnz",
    // I/O
    "in","inb","inw","inl","out","outb","outw","outl",
    // Misc / control
    "hlt","pause","wait","fwait","lock","xchg","xadd","cmpxchg","cmpxchg8b","cmpxchg16b",
    "lfence","mfence","sfence","clflush","clflushopt","clwb",
    "cpuid","rdtsc","rdtscp","rdmsr","wrmsr","rdpmc",
    "lgdt","sgdt","lidt","sidt","lldt","sldt","ltr","str",
    "lmsw","smsw","clts","invd","wbinvd","invlpg",
    "lar","lsl","verr","verw","arpl",
    "int","int3","into","bound","ud2","ud1",
    "stc","clc","cmc","std","cld","sti","cli",
    "lahf","sahf","pushf","pushfw","pushfq","popf","popfw","popfq",
    "xlat","xlatb",
    "xsave","xsavec","xsaveopt","xrstor","xgetbv","xsetbv",
    // Address / misc
    "nop2","nop3","nop4","nop5","nop6","nop7","nop8","nop9",
    // SSE / SSE2 scalar & packed (common subset)
    "addss","addsd","addps","addpd","subss","subsd","subps","subpd",
    "mulss","mulsd","mulps","mulpd","divss","divsd","divps","divpd",
    "sqrtss","sqrtsd","sqrtps","sqrtpd","rcpss","rcpps","rsqrtss","rsqrtps",
    "maxss","maxsd","maxps","maxpd","minss","minsd","minps","minpd",
    "cmpss","cmpsd","cmpps","cmppd","ucomiss","ucomisd","comiss","comisd",
    "cvtss2sd","cvtsd2ss","cvtss2si","cvtsd2si","cvtsi2ss","cvtsi2sd",
    "cvtps2pd","cvtpd2ps","cvtps2dq","cvtdq2ps","cvtpd2dq","cvtdq2pd",
    "cvttss2si","cvttsd2si","cvttps2dq","cvttpd2dq",
    "andps","andpd","andnps","andnpd","orps","orpd","xorps","xorpd",
    "unpcklps","unpckhps","unpcklpd","unpckhpd",
    "shufps","shufpd","pshufd","pshufhw","pshuflw","pshufw",
    "movlps","movhps","movlpd","movhpd","movmskps","movmskpd",
    "movntps","movntpd","movnti","movntq","movntdq",
    "ldmxcsr","stmxcsr","prefetcht0","prefetcht1","prefetcht2","prefetchnta",
    // SSE integer
    "paddb","paddw","paddd","paddq","paddsb","paddsw","paddusb","paddusw",
    "psubb","psubw","psubd","psubq","psubsb","psubsw","psubusb","psubusw",
    "pmullw","pmulhw","pmulhuw","pmulld","pmuludq","pmuldq",
    "pcmpeqb","pcmpeqw","pcmpeqd","pcmpeqq",
    "pcmpgtb","pcmpgtw","pcmpgtd","pcmpgtq",
    "pand","pandn","por","pxor","pnot",
    "psllw","pslld","psllq","psrlw","psrld","psrlq","psraw","psrad",
    "punpcklbw","punpcklwd","punpckldq","punpcklqdq",
    "punpckhbw","punpckhwd","punpckhdq","punpckhqdq",
    "packuswb","packusdw","packsswb","packssdw",
    "pmaxsb","pmaxsw","pmaxsd","pmaxub","pmaxuw","pmaxud",
    "pminsb","pminsw","pminsd","pminub","pminuw","pminud",
    "pextrb","pextrw","pextrd","pextrq","pinsrb","pinsrw","pinsrd","pinsrq",
    "pmovmskb","movdq2q","movq2dq",
    "palignr","pblendw","pblendvb","blendps","blendpd","blendvps","blendvpd",
    "ptest","vtestps","vtestpd",
    // AVX (v-prefix representative)
    "vmovaps","vmovups","vmovapd","vmovupd","vmovdqa","vmovdqu",
    "vaddps","vaddpd","vsubps","vsubpd","vmulps","vmulpd","vdivps","vdivpd",
    "vxorps","vxorpd","vandps","vandpd","vorps","vorpd",
    "vpxor","vpand","vpor","vpandn","vpcmpeqb","vpcmpeqd",
    "vbroadcastss","vbroadcastsd","vbroadcasti128",
    "vperm2f128","vperm2i128","vpermq","vpermd","vpermps",
    "vinsertf128","vinserti128","vextractf128","vextracti128",
    "vzeroall","vzeroupper",
    // FPU x87
    "fld","flds","fldl","fldt","fld1","fldz","fldpi","fldl2e","fldl2t","fldlg2","fldln2",
    "fst","fsts","fstl","fstp","fstps","fstpl","fstpt",
    "fadd","fadds","faddl","faddp","fiadd","fiadds","fiaddl",
    "fsub","fsubs","fsubl","fsubp","fsubr","fsubrs","fsubrl","fsubrp",
    "fisub","fisubs","fisubl","fisubr","fisubrs","fisubrl",
    "fmul","fmuls","fmull","fmulp","fimul","fimuls","fimull",
    "fdiv","fdivs","fdivl","fdivp","fdivr","fdivrs","fdivrl","fdivrp",
    "fidiv","fidivs","fidivl","fidivr","fidivrs","fidivrl",
    "fcom","fcoms","fcoml","fcomp","fcomps","fcompl","fcompp",
    "fucom","fucomp","fucompp","fxam","ftst","fabs","fchs","frndint",
    "fsqrt","fscale","fxtract","fprem","fprem1","f2xm1","fyl2x","fyl2xp1",
    "fptan","fpatan","fsin","fcos","fsincos",
    "finit","fninit","fldcw","fstcw","fnstcw","fstenv","fnstenv",
    "fldenv","frstor","fsave","fnsave","fwait","fnwait",
    "ffree","fdecstp","fincstp","fnop",
    "fxsave","fxrstor",
    nullptr
};
// x86-64 registers (AT&T prefix % stripped before lookup)
static const char* const REG_KW[] = {
    // 64-bit
    "rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp",
    "r8","r9","r10","r11","r12","r13","r14","r15",
    // 32-bit
    "eax","ebx","ecx","edx","esi","edi","ebp","esp",
    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d",
    // 16-bit
    "ax","bx","cx","dx","si","di","bp","sp",
    "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w",
    // 8-bit
    "al","bl","cl","dl","sil","dil","bpl","spl",
    "ah","bh","ch","dh",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b",
    // segment / control / debug
    "cs","ds","es","fs","gs","ss",
    "cr0","cr2","cr3","cr4","cr8",
    "dr0","dr1","dr2","dr3","dr6","dr7",
    // XMM / YMM / ZMM (representative subset)
    "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7",
    "xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
    "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7",
    "ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
    "zmm0","zmm1","zmm2","zmm3","zmm4","zmm5","zmm6","zmm7",
    "zmm8","zmm9","zmm10","zmm11","zmm12","zmm13","zmm14","zmm15",
    // FPU / MMX
    "st","st0","st1","st2","st3","st4","st5","st6","st7",
    "mm0","mm1","mm2","mm3","mm4","mm5","mm6","mm7",
    nullptr
};
// Assembler directives
static const char* const DIR_KW[] = {
    // GAS directives
    ".align",".ascii",".asciz",".byte",".comm",".data",".double",".else",".endif",
    ".equ",".extern",".file",".float",".global",".globl",".hidden",".ident",
    ".if",".ifdef",".ifndef",".include",".int",".intel_syntax",".att_syntax",
    ".long",".local",".macro",".noprefix",".octa",".org",".p2align",
    ".popsection",".previous",".protected",".quad",".rept",".rodata",
    ".section",".set",".short",".size",".skip",".sleb128",".space",".string",
    ".struct",".subsection",".symver",".text",".title",".type",".uleb128",
    ".value",".version",".vtable_entry",".vtable_inherit",".weak",".word",
    ".zero",".2byte",".4byte",".8byte",
    // NASM / MASM directives
    "db","dw","dd","dq","dt","do","dy","dz",
    "resb","resw","resd","resq","rest","reso","resy","resz",
    "equ","times","bits","use16","use32","use64",
    "segment","section","global","extern","common","struc","endstruc",
    ".model",".stack",".code",".data","proc","endp","end","assume",
    "dword","qword","byte","word","ptr","offset","flat",
    nullptr
};

// Build a lookup table from keyword array
static std::unordered_map<std::string, TT> buildKwMap() {
    std::unordered_map<std::string, TT> m;
    m.reserve(600); // bumped to accommodate INSTR_KW
    auto add = [&](const char* const* arr, TT t) {
        for (int i = 0; arr[i]; ++i) m[arr[i]] = t;
    };
    add(REG_KW,      TT::REGISTER);
    add(DIR_KW,      TT::DIRECTIVE);
    add(NOP_KW,      TT::NOP);
    add(STACK_KW,    TT::STACK);
    add(CALL_RET_KW, TT::CALL_RET);
    add(JUMP_KW,     TT::JUMP);
    add(MOV_LEA_KW,  TT::MOV_LEA);
    add(INSTR_KW,    TT::MOV_LEA);  // FIX: was missing — all general instructions now colored
    // rip/flags override register
    m["rip"]    = TT::RIP;
    m["eip"]    = TT::RIP;
    m["ip"]     = TT::RIP;
    m["rflags"] = TT::RIP;
    m["eflags"] = TT::RIP;
    return m;
}

// ---------------------------------------------------------------------------
// Span  – a coloured run within a single line
// ---------------------------------------------------------------------------
struct Span {
    int   col;   // column (char offset) within the line
    int   len;
    TT    type;
    bool  bold;
};

// ---------------------------------------------------------------------------
// Tokenizer  – operates on a single line (std::string_view)
// ---------------------------------------------------------------------------
static const std::unordered_map<std::string, TT>& kwMap() {
    static auto m = buildKwMap();
    return m;
}

static bool isIdentStart(char c) {
    return std::isalpha((unsigned char)c) || c == '_' || c == '.' || c == '@' || c == '?';
}
static bool isIdentCont(char c) {
    return std::isalnum((unsigned char)c) || c == '_' || c == '.' || c == '@' || c == '?';
}
static bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

// Returns lowercase copy
static std::string toLower(std::string_view sv) {
    std::string s(sv);
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::vector<Span> tokenizeLine(std::string_view line) {
    std::vector<Span> spans;
    const auto& km = kwMap();
    int n = (int)line.size();
    int i = 0;

    // Helper: push span
    auto push = [&](int col, int len, TT t) {
        if (len > 0)
            spans.push_back({col, len, t, tokenBold(t)});
    };

    while (i < n) {
        // --- Whitespace ---
        if (std::isspace((unsigned char)line[i])) { ++i; continue; }

        // --- Comments: ; // # ---
        if (line[i] == ';' || (line[i] == '/' && i+1 < n && line[i+1] == '/')) {
            push(i, n - i, TT::COMMENT);
            break;
        }
        if (line[i] == '#') {
            push(i, n - i, TT::COMMENT);
            break;
        }
        // Block comment start /* (mark rest of line)
        if (line[i] == '/' && i+1 < n && line[i+1] == '*') {
            push(i, n - i, TT::COMMENT);
            break;
        }

        // --- String literals: " and ' ---
        if (line[i] == '"' || line[i] == '\'') {
            char q = line[i];
            int start = i++;
            while (i < n) {
                if (line[i] == '\\') { i += 2; continue; }
                if (line[i] == q)    { ++i; break; }
                ++i;
            }
            push(start, i - start, TT::STRING);
            continue;
        }

        // --- AT&T register prefix % ---
        if (line[i] == '%') {
            int start = i++;
            while (i < n && isIdentCont(line[i])) ++i;
            if (i > start + 1) {
                auto name = toLower(line.substr(start + 1, i - start - 1));
                auto it = km.find(name);
                TT t = TT::REGISTER;
                if (it != km.end() && (it->second == TT::REGISTER || it->second == TT::RIP))
                    t = it->second;
                push(start, i - start, t);
            } else {
                push(start, 1, TT::DEFAULT);
            }
            continue;
        }

        // --- Numeric constants ---
        if (std::isdigit((unsigned char)line[i]) ||
            (line[i] == '-' && i+1 < n && std::isdigit((unsigned char)line[i+1]))) {
            int start = i;
            if (line[i] == '-') ++i;
            if (line[i] == '0' && i+1 < n && (line[i+1]=='x'||line[i+1]=='X')) {
                i += 2;
                while (i < n && isHexDigit(line[i])) ++i;
            } else if (line[i] == '0' && i+1 < n && (line[i+1]=='b'||line[i+1]=='B')) {
                i += 2;
                while (i < n && (line[i]=='0'||line[i]=='1')) ++i;
            } else {
                while (i < n && std::isalnum((unsigned char)line[i])) ++i;
            }
            push(start, i - start, TT::CONSTANT);
            continue;
        }

        // --- $ constant (AT&T immediate / NASM $/$$ / MASM offset) ---
        if (line[i] == '$') {
            int start = i++;
            if (i < n && std::isdigit((unsigned char)line[i])) {
                while (i < n && std::isalnum((unsigned char)line[i])) ++i;
                push(start, i - start, TT::CONSTANT);
            } else if (i < n && (line[i] == '$' || std::isalpha((unsigned char)line[i]) || line[i]=='_')) {
                while (i < n && isIdentCont(line[i])) ++i;
                push(start, i - start, TT::CONSTANT);
            } else {
                push(start, 1, TT::DEFAULT);
            }
            continue;
        }

        // --- Brackets [ ] ---  FIX: was falling through to DEFAULT
        if (line[i] == '[' || line[i] == ']') {
            push(i, 1, TT::BRACKET);
            ++i;
            continue;
        }

        // --- Identifier / keyword / label ---
        if (isIdentStart(line[i])) {
            int start = i++;
            while (i < n && isIdentCont(line[i])) ++i;

            // Check for label: identifier followed by ':'
            {
                int j = i;
                while (j < n && line[j] == ' ') ++j;
                if (j < n && line[j] == ':') {
                    push(start, j - start + 1, TT::LABEL);
                    i = j + 1;
                    continue;
                }
            }

            auto raw = line.substr(start, i - start);
            auto low = toLower(raw);

            // Section names: .text .data .bss etc.
            if (raw[0] == '.') {
                auto it = km.find(low);
                if (it != km.end() && it->second == TT::DIRECTIVE) {
                    bool isSect = (low == ".text" || low == ".data" || low == ".bss"   ||
                                   low == ".rdata"|| low == ".rodata"|| low == ".code" ||
                                   low == ".const"|| low == ".tls"  || low == ".rsrc"  ||
                                   low == ".reloc"|| low == ".debug"|| low == ".pdata" ||
                                   low == ".xdata");
                    spans.push_back({start, i - start, isSect ? TT::SECTION : TT::DIRECTIVE, isSect});
                } else {
                    spans.push_back({start, i - start, TT::DIRECTIVE, false});
                }
                continue;
            }

            auto it = km.find(low);
            if (it != km.end()) {
                spans.push_back({start, i - start, it->second, tokenBold(it->second)});
            } else {
                push(start, i - start, TT::DEFAULT);
            }
            continue;
        }

        // --- Operators / punctuation: single char default ---
        push(i, 1, TT::DEFAULT);
        ++i;
    }
    return spans;
}

// ---------------------------------------------------------------------------
// Editor state
// ---------------------------------------------------------------------------
struct Editor {
    std::vector<std::string> lines;

    int   curLine   = 0;
    int   curCol    = 0;

    int   selLine   = 0;
    int   selCol    = 0;
    bool  hasSelection = false;

    int   scrollLine = 0;
    int   scrollCol  = 0;

    int   charW  = 0;
    int   charH  = 0;
    int   lineNumW = 0;
    int   visLines = 0;
    int   visChars = 0;

    HWND  hwnd = nullptr;

    HFONT font     = nullptr;
    HFONT fontBold = nullptr;

    wchar_t filePath[MAX_PATH] = {};

    bool modified = false;

    struct Snapshot {
        std::vector<std::string> lines;
        int curLine, curCol;
    };
    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    static constexpr int MAX_UNDO = 200;

    void pushUndo() {
        if ((int)undoStack.size() >= MAX_UNDO) undoStack.erase(undoStack.begin());
        undoStack.push_back({lines, curLine, curCol});
        redoStack.clear();
        modified = true;
    }

    void undo() {
        if (undoStack.empty()) return;
        redoStack.push_back({lines, curLine, curCol});
        auto& snap = undoStack.back();
        lines   = snap.lines;
        curLine = snap.curLine;
        curCol  = snap.curCol;
        undoStack.pop_back();
        clampCursor();
    }

    void redo() {
        if (redoStack.empty()) return;
        undoStack.push_back({lines, curLine, curCol});
        auto& snap = redoStack.back();
        lines   = snap.lines;
        curLine = snap.curLine;
        curCol  = snap.curCol;
        redoStack.pop_back();
        clampCursor();
    }

    void clampCursor() {
        curLine = std::clamp(curLine, 0, (int)lines.size()-1);
        curCol  = std::clamp(curCol, 0, (int)lines[curLine].size());
    }

    void ensureCursorVisible() {
        if (curLine < scrollLine) scrollLine = curLine;
        if (curLine >= scrollLine + visLines - 1) scrollLine = curLine - visLines + 2;
        if (scrollLine < 0) scrollLine = 0;
        if (curCol < scrollCol) scrollCol = curCol;
        if (curCol >= scrollCol + visChars - 1) scrollCol = curCol - visChars + 2;
        if (scrollCol < 0) scrollCol = 0;
    }

    RECT caretRect(int ln, int col) const {
        RECT r;
        r.top    = (ln - scrollLine) * charH;
        r.bottom = r.top + charH;
        r.left   = lineNumW + (col - scrollCol) * charW;
        r.right  = r.left + 2;
        return r;
    }

    void posFromPoint(int x, int y, int& ln, int& col) const {
        ln = y / charH + scrollLine;
        ln = std::clamp(ln, 0, (int)lines.size()-1);
        col = (x - lineNumW) / charW + scrollCol;
        col = std::clamp(col, 0, (int)lines[ln].size());
    }

    void selNorm(int& aL, int& aC, int& bL, int& bC) const {
        if (aL > bL || (aL == bL && aC > bC)) {
            std::swap(aL, bL);
            std::swap(aC, bC);
        }
    }

    bool selEmpty() const {
        return !hasSelection || (curLine == selLine && curCol == selCol);
    }

    std::string selText() const {
        if (selEmpty()) return {};
        int aL = selLine, aC = selCol, bL = curLine, bC = curCol;
        selNorm(aL, aC, bL, bC);
        std::string out;
        for (int li = aL; li <= bL; ++li) {
            int cs = (li == aL) ? aC : 0;
            int ce = (li == bL) ? bC : (int)lines[li].size();
            out += lines[li].substr(cs, ce - cs);
            if (li < bL) out += '\n';
        }
        return out;
    }

    void deleteSelection() {
        if (selEmpty()) return;
        int aL = selLine, aC = selCol, bL = curLine, bC = curCol;
        selNorm(aL, aC, bL, bC);
        pushUndo();
        std::string head = lines[aL].substr(0, aC);
        std::string tail = lines[bL].substr(bC);
        lines.erase(lines.begin() + aL, lines.begin() + bL + 1);
        lines.insert(lines.begin() + aL, head + tail);
        curLine = aL; curCol = aC;
        hasSelection = false;
    }

    void selAnchor() {
        selLine = curLine;
        selCol  = curCol;
        hasSelection = true;
    }

    void copyToClipboard(HWND hwnd) const {
        if (selEmpty()) return;
        std::string txt = selText();
        std::string out;
        out.reserve(txt.size() + 32);
        for (char c : txt) {
            if (c == '\n') out += '\r';
            out += c;
        }
        if (!OpenClipboard(hwnd)) return;
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, out.size() + 1);
        if (hg) {
            memcpy(GlobalLock(hg), out.c_str(), out.size() + 1);
            GlobalUnlock(hg);
            SetClipboardData(CF_TEXT, hg);
        }
        CloseClipboard();
    }

    void pasteFromClipboard(HWND hwnd) {
        if (!OpenClipboard(hwnd)) return;
        HGLOBAL hg = GetClipboardData(CF_TEXT);
        if (!hg) { CloseClipboard(); return; }
        const char* ptr = (const char*)GlobalLock(hg);
        std::string txt(ptr);
        GlobalUnlock(hg);
        CloseClipboard();

        if (!selEmpty()) deleteSelection();
        pushUndo();
        for (char c : txt) {
            if (c == '\r') continue;
            if (c == '\n') {
                std::string rest = lines[curLine].substr(curCol);
                lines[curLine].erase(curCol);
                lines.insert(lines.begin() + curLine + 1, rest);
                ++curLine; curCol = 0;
            } else {
                lines[curLine].insert(lines[curLine].begin() + curCol, c);
                ++curCol;
            }
        }
        hasSelection = false;
    }
};

static Editor g_ed;

// ---------------------------------------------------------------------------
// Font creation
// ---------------------------------------------------------------------------
static HFONT makeFont(int height, bool bold) {
    return CreateFontW(
        height, 0, 0, 0,
        bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FIXED_PITCH | FF_MODERN,
        L"Cascadia Mono"
    );
}

static HFONT makeFont2(int height, bool bold) {
    HFONT f = makeFont(height, bold);
    LOGFONTW lf{};
    GetObjectW(f, sizeof(lf), &lf);
    if (std::wstring(lf.lfFaceName).find(L"Cascadia") == std::wstring::npos) {
        DeleteObject(f);
        return CreateFontW(height, 0, 0, 0,
            bold ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN,
            L"Consolas");
    }
    return f;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------
static void updateScrollBars(HWND hwnd) {
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_ALL | SIF_DISABLENOSCROLL;

    si.nMin  = 0;
    si.nMax  = (int)g_ed.lines.size() - 1;
    si.nPage = g_ed.visLines;
    si.nPos  = g_ed.scrollLine;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    int maxLen = 0;
    for (auto& l : g_ed.lines) maxLen = std::max(maxLen, (int)l.size());
    si.nMin  = 0;
    si.nMax  = maxLen;
    si.nPage = g_ed.visChars;
    si.nPos  = g_ed.scrollCol;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

static void onPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT client;
    GetClientRect(hwnd, &client);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    SetBkMode(memDC, TRANSPARENT);

    HBRUSH bgBrush = CreateSolidBrush(Col::BG);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);

    SelectObject(memDC, g_ed.font);
    TEXTMETRICW tm{};
    GetTextMetricsW(memDC, &tm);
    g_ed.charW = tm.tmAveCharWidth;
    g_ed.charH = tm.tmHeight + tm.tmExternalLeading;
    g_ed.visLines = (client.bottom / g_ed.charH) + 1;
    g_ed.visChars = ((client.right - g_ed.lineNumW) / g_ed.charW) + 1;

    int numDigits = 1;
    for (int x = (int)g_ed.lines.size(); x >= 10; x /= 10) ++numDigits;
    g_ed.lineNumW = (numDigits + 2) * g_ed.charW;

    RECT gutterRect = {0, 0, g_ed.lineNumW - g_ed.charW/2, client.bottom};
    HBRUSH gutterBrush = CreateSolidBrush(Col::LINE_NUM_BG);
    FillRect(memDC, &gutterRect, gutterBrush);
    DeleteObject(gutterBrush);

    // Current-line highlight
    {
        int cy = (g_ed.curLine - g_ed.scrollLine) * g_ed.charH;
        RECT hlRect = {0, cy, client.right, cy + g_ed.charH};
        HBRUSH hlBrush = CreateSolidBrush(Col::CURRENT_LINE);
        FillRect(memDC, &hlRect, hlBrush);
        DeleteObject(hlBrush);
    }

    // Selection highlight
    if (!g_ed.selEmpty()) {
        int aL = g_ed.selLine, aC = g_ed.selCol;
        int bL = g_ed.curLine, bC = g_ed.curCol;
        g_ed.selNorm(aL, aC, bL, bC);
        HBRUSH selBrush = CreateSolidBrush(Col::SEL_BG);
        for (int li = std::max(aL, g_ed.scrollLine);
             li <= std::min(bL, g_ed.scrollLine + g_ed.visLines + 1); ++li) {
            int yPx  = (li - g_ed.scrollLine) * g_ed.charH;
            int cStart = (li == aL) ? aC : 0;
            int cEnd   = (li == bL) ? bC : (int)g_ed.lines[li].size();
            int xStart = g_ed.lineNumW + (cStart - g_ed.scrollCol) * g_ed.charW;
            int xEnd   = (li < bL)
                ? client.right
                : g_ed.lineNumW + (cEnd - g_ed.scrollCol) * g_ed.charW;
            xStart = std::max(xStart, g_ed.lineNumW);
            xEnd   = std::max(xEnd,   g_ed.lineNumW);
            if (xEnd > xStart) {
                RECT sr = {xStart, yPx, xEnd, yPx + g_ed.charH};
                FillRect(memDC, &sr, selBrush);
            }
        }
        DeleteObject(selBrush);
    }

    int firstLine = g_ed.scrollLine;
    int lastLine  = std::min((int)g_ed.lines.size()-1, firstLine + g_ed.visLines + 1);

    for (int li = firstLine; li <= lastLine; ++li) {
        int yPx = (li - g_ed.scrollLine) * g_ed.charH;
        const std::string& lineStr = g_ed.lines[li];

        // Line number
        {
            char buf[16];
            int nc = snprintf(buf, sizeof(buf), "%*d", numDigits, li + 1);
            SetTextColor(memDC, Col::LINE_NUM_FG);
            SelectObject(memDC, g_ed.font);
            TextOutA(memDC, g_ed.charW/2, yPx, buf, nc);
        }

        if (lineStr.empty()) continue;

        auto spans = tokenizeLine(lineStr);

        std::vector<TT>   types(lineStr.size(), TT::DEFAULT);
        std::vector<bool> bolds(lineStr.size(), false);
        for (auto& sp : spans) {
            int end = std::min(sp.col + sp.len, (int)lineStr.size());
            for (int ci = sp.col; ci < end; ++ci) {
                types[ci] = sp.type;
                bolds[ci] = sp.bold;
            }
        }

        int ci = g_ed.scrollCol;
        int maxCi = std::min((int)lineStr.size(), g_ed.scrollCol + g_ed.visChars + 1);
        while (ci < maxCi) {
            TT   curType = types[ci];
            bool curBold = bolds[ci];
            int  runStart = ci;
            while (ci < maxCi && types[ci] == curType && bolds[ci] == curBold) ++ci;
            int runLen = ci - runStart;
            if (runLen <= 0) continue;

            int xPx = g_ed.lineNumW + (runStart - g_ed.scrollCol) * g_ed.charW;
            SetTextColor(memDC, tokenColor(curType));
            SelectObject(memDC, curBold ? g_ed.fontBold : g_ed.font);
            TextOutA(memDC, xPx, yPx, lineStr.c_str() + runStart, runLen);
        }
    }

    // Caret
    {
        RECT cr = g_ed.caretRect(g_ed.curLine, g_ed.curCol);
        HBRUSH caretBrush = CreateSolidBrush(Col::CARET);
        RECT caretFill = {cr.left, cr.top, cr.left+2, cr.bottom};
        FillRect(memDC, &caretFill, caretBrush);
        DeleteObject(caretBrush);
    }

    BitBlt(hdc, 0, 0, client.right, client.bottom, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
    updateScrollBars(hwnd);
}

// ---------------------------------------------------------------------------
// Text manipulation helpers
// ---------------------------------------------------------------------------
static void insertChar(Editor& ed, char c) {
    if (!ed.selEmpty()) ed.deleteSelection();
    ed.pushUndo();
    auto& line = ed.lines[ed.curLine];
    line.insert(line.begin() + ed.curCol, c);
    ++ed.curCol;
}

static void insertNewline(Editor& ed) {
    if (!ed.selEmpty()) ed.deleteSelection();
    ed.pushUndo();
    auto& line = ed.lines[ed.curLine];
    std::string rest = line.substr(ed.curCol);
    line.erase(ed.curCol);

    std::string indent;
    for (char c : line) {
        if (c == ' ' || c == '\t') indent += c;
        else break;
    }

    ed.lines.insert(ed.lines.begin() + ed.curLine + 1, indent + rest);
    ++ed.curLine;
    ed.curCol = (int)indent.size();
}

static void deleteCharBack(Editor& ed) {
    if (!ed.selEmpty()) { ed.deleteSelection(); return; }
    if (ed.curCol > 0) {
        ed.pushUndo();
        auto& line = ed.lines[ed.curLine];
        line.erase(line.begin() + ed.curCol - 1);
        --ed.curCol;
    } else if (ed.curLine > 0) {
        ed.pushUndo();
        std::string cur = ed.lines[ed.curLine];
        ed.lines.erase(ed.lines.begin() + ed.curLine);
        --ed.curLine;
        auto& prev = ed.lines[ed.curLine];
        ed.curCol = (int)prev.size();
        prev += cur;
    }
}

static void deleteCharFwd(Editor& ed) {
    if (!ed.selEmpty()) { ed.deleteSelection(); return; }
    auto& line = ed.lines[ed.curLine];
    if (ed.curCol < (int)line.size()) {
        ed.pushUndo();
        line.erase(line.begin() + ed.curCol);
    } else if (ed.curLine + 1 < (int)ed.lines.size()) {
        ed.pushUndo();
        std::string next = ed.lines[ed.curLine + 1];
        ed.lines.erase(ed.lines.begin() + ed.curLine + 1);
        line += next;
    }
}

// ---------------------------------------------------------------------------
// File I/O
// ---------------------------------------------------------------------------
static void loadFile(Editor& ed, const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    if (sz.QuadPart > 64*1024*1024) { CloseHandle(h); return; }

    std::string buf((size_t)sz.QuadPart, '\0');
    DWORD read = 0;
    ReadFile(h, buf.data(), (DWORD)sz.QuadPart, &read, nullptr);
    CloseHandle(h);

    ed.lines.clear();
    ed.undoStack.clear();
    ed.redoStack.clear();
    ed.curLine = ed.curCol = 0;
    ed.scrollLine = ed.scrollCol = 0;
    ed.modified = false;
    wcscpy_s(ed.filePath, path);

    std::string line;
    for (char c : buf) {
        if (c == '\r') continue;
        if (c == '\n') { ed.lines.push_back(line); line.clear(); }
        else line += c;
    }
    ed.lines.push_back(line);
    if (ed.lines.empty()) ed.lines.push_back("");
}

static void saveFile(Editor& ed, const wchar_t* path) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    for (size_t i = 0; i < ed.lines.size(); ++i) {
        DWORD w;
        WriteFile(h, ed.lines[i].c_str(), (DWORD)ed.lines[i].size(), &w, nullptr);
        if (i + 1 < ed.lines.size()) WriteFile(h, "\n", 1, &w, nullptr);
    }
    CloseHandle(h);
    wcscpy_s(ed.filePath, path);
    ed.modified = false;
}

// ---------------------------------------------------------------------------
// Menu IDs
// ---------------------------------------------------------------------------
enum {
    ID_FILE_NEW  = 1001,
    ID_FILE_OPEN,
    ID_FILE_SAVE,
    ID_FILE_SAVEAS,
    ID_FILE_EXIT,
    ID_EDIT_UNDO,
    ID_EDIT_REDO,
    ID_EDIT_CUT,
    ID_EDIT_COPY,
    ID_EDIT_PASTE,
    ID_EDIT_SELECTALL,
    ID_VIEW_ZOOM_IN,
    ID_VIEW_ZOOM_OUT,
};

static HMENU buildMenu() {
    HMENU bar  = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_FILE_NEW,    L"&New\tCtrl+N");
    AppendMenuW(file, MF_STRING, ID_FILE_OPEN,   L"&Open...\tCtrl+O");
    AppendMenuW(file, MF_STRING, ID_FILE_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(file, MF_STRING, ID_FILE_SAVEAS, L"Save &As...");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_EXIT,   L"E&xit\tAlt+F4");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file,   L"&File");

    HMENU edit = CreatePopupMenu();
    AppendMenuW(edit, MF_STRING, ID_EDIT_UNDO,      L"&Undo\tCtrl+Z");
    AppendMenuW(edit, MF_STRING, ID_EDIT_REDO,      L"&Redo\tCtrl+Y");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, ID_EDIT_CUT,       L"Cu&t\tCtrl+X");
    AppendMenuW(edit, MF_STRING, ID_EDIT_COPY,      L"&Copy\tCtrl+C");
    AppendMenuW(edit, MF_STRING, ID_EDIT_PASTE,     L"&Paste\tCtrl+V");
    AppendMenuW(edit, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(edit, MF_STRING, ID_EDIT_SELECTALL, L"Select &All\tCtrl+A");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)edit,      L"&Edit");

    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, ID_VIEW_ZOOM_IN,  L"Zoom &In\tCtrl++");
    AppendMenuW(view, MF_STRING, ID_VIEW_ZOOM_OUT, L"Zoom &Out\tCtrl+-");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view,   L"&View");

    return bar;
}

// ---------------------------------------------------------------------------
// Window title helper
// ---------------------------------------------------------------------------
static void updateTitle(HWND hwnd) {
    wchar_t title[MAX_PATH + 32];
    const wchar_t* name = g_ed.filePath[0] ? g_ed.filePath : L"Untitled";
    const wchar_t* slash = std::max(wcsrchr(name, L'\\'), wcsrchr(name, L'/'));
    if (slash) name = slash + 1;
    swprintf_s(title, L"%s%s - ASM Editor", g_ed.modified ? L"* " : L"", name);
    SetWindowTextW(hwnd, title);
}

// ---------------------------------------------------------------------------
// Font size control
// ---------------------------------------------------------------------------
static int g_fontSize = 16;

static void recreateFonts() {
    if (g_ed.font)     { DeleteObject(g_ed.font);     g_ed.font = nullptr; }
    if (g_ed.fontBold) { DeleteObject(g_ed.fontBold); g_ed.fontBold = nullptr; }
    g_ed.font     = makeFont2(g_fontSize, false);
    g_ed.fontBold = makeFont2(g_fontSize, true);
}

// ---------------------------------------------------------------------------
// WndProc
// ---------------------------------------------------------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Editor& ed = g_ed;

    switch (msg) {

    case WM_CREATE: {
        ed.hwnd = hwnd;
        ed.lines.push_back("");
        recreateFonts();
        SetMenu(hwnd, buildMenu());
        CreateCaret(hwnd, nullptr, 2, g_fontSize + 2);
        ShowCaret(hwnd);
        return 0;
    }

    case WM_DESTROY:
        DestroyCaret();
        if (ed.font)     DeleteObject(ed.font);
        if (ed.fontBold) DeleteObject(ed.fontBold);
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        RECT r;
        GetClientRect(hwnd, &r);
        ed.visLines = r.bottom / (ed.charH ? ed.charH : g_fontSize) + 1;
        ed.visChars = (r.right - ed.lineNumW) / (ed.charW ? ed.charW : g_fontSize/2) + 1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_SETFOCUS:
        CreateCaret(hwnd, nullptr, 2, ed.charH ? ed.charH : g_fontSize);
        ShowCaret(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_KILLFOCUS:
        DestroyCaret();
        return 0;

    case WM_PAINT: {
        onPaint(hwnd);
        RECT cr = ed.caretRect(ed.curLine, ed.curCol);
        SetCaretPos(cr.left, cr.top);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si{sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wParam)) {
            case SB_LINEUP:     --ed.scrollLine; break;
            case SB_LINEDOWN:   ++ed.scrollLine; break;
            case SB_PAGEUP:     ed.scrollLine -= ed.visLines; break;
            case SB_PAGEDOWN:   ed.scrollLine += ed.visLines; break;
            case SB_THUMBTRACK: ed.scrollLine = si.nTrackPos; break;
            case SB_TOP:        ed.scrollLine = 0; break;
            case SB_BOTTOM:     ed.scrollLine = (int)ed.lines.size()-1; break;
        }
        ed.scrollLine = std::clamp(ed.scrollLine, 0, (int)ed.lines.size()-1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_HSCROLL: {
        SCROLLINFO si{sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd, SB_HORZ, &si);
        switch (LOWORD(wParam)) {
            case SB_LINELEFT:   --ed.scrollCol; break;
            case SB_LINERIGHT:  ++ed.scrollCol; break;
            case SB_PAGELEFT:   ed.scrollCol -= ed.visChars; break;
            case SB_PAGERIGHT:  ed.scrollCol += ed.visChars; break;
            case SB_THUMBTRACK: ed.scrollCol = si.nTrackPos; break;
        }
        ed.scrollCol = std::max(0, ed.scrollCol);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        ed.scrollLine -= delta / WHEEL_DELTA * 3;
        ed.scrollLine = std::clamp(ed.scrollLine, 0, (int)ed.lines.size()-1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        SetFocus(hwnd);
        int x = LOWORD(lParam), y = HIWORD(lParam);
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shift) {
            ed.posFromPoint(x, y, ed.curLine, ed.curCol);
            ed.selAnchor();
            ed.hasSelection = false;
        } else {
            ed.posFromPoint(x, y, ed.curLine, ed.curCol);
            ed.hasSelection = true;
        }
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (wParam & MK_LBUTTON) {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            ed.posFromPoint(x, y, ed.curLine, ed.curCol);
            ed.hasSelection = true;
            ed.ensureCursorVisible();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        ReleaseCapture();
        if (ed.curLine == ed.selLine && ed.curCol == ed.selCol)
            ed.hasSelection = false;
        return 0;

    case WM_CHAR: {
        char c = (char)wParam;
        if (c == '\b') {
            deleteCharBack(ed);
        } else if (c == '\r') {
            insertNewline(ed);
        } else if (c == '\t') {
            if (!ed.selEmpty()) ed.deleteSelection();
            for (int i = 0; i < 4; ++i) insertChar(ed, ' ');
        } else if ((unsigned char)c >= 32) {
            insertChar(ed, c);
        }
        ed.ensureCursorVisible();
        updateTitle(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_KEYDOWN: {
        bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;

        auto moveSel = [&]() {
            if (shift) {
                if (!ed.hasSelection) ed.selAnchor();
            } else {
                ed.hasSelection = false;
            }
        };

        if (ctrl) {
            switch (wParam) {
                case 'Z': ed.undo(); updateTitle(hwnd); break;
                case 'Y': ed.redo(); updateTitle(hwnd); break;
                case 'A': {
                    ed.selLine = 0; ed.selCol = 0;
                    ed.curLine = (int)ed.lines.size()-1;
                    ed.curCol  = (int)ed.lines[ed.curLine].size();
                    ed.hasSelection = true;
                    break;
                }
                case 'C': ed.copyToClipboard(hwnd); return 0;
                case 'X':
                    ed.copyToClipboard(hwnd);
                    ed.deleteSelection();
                    updateTitle(hwnd);
                    break;
                case 'V':
                    ed.pasteFromClipboard(hwnd);
                    updateTitle(hwnd);
                    break;
                case 'S': {
                    if (ed.filePath[0]) saveFile(ed, ed.filePath);
                    else SendMessageW(hwnd, WM_COMMAND, ID_FILE_SAVEAS, 0);
                    updateTitle(hwnd);
                    return 0;
                }
                case 'N': SendMessageW(hwnd, WM_COMMAND, ID_FILE_NEW,  0); return 0;
                case 'O': SendMessageW(hwnd, WM_COMMAND, ID_FILE_OPEN, 0); return 0;
                case VK_OEM_PLUS:  case VK_ADD:
                    g_fontSize = std::min(72, g_fontSize + 2);
                    recreateFonts();
                    break;
                case VK_OEM_MINUS: case VK_SUBTRACT:
                    g_fontSize = std::max(8, g_fontSize - 2);
                    recreateFonts();
                    break;
                case VK_HOME:
                    moveSel();
                    ed.curLine = 0; ed.curCol = 0;
                    break;
                case VK_END:
                    moveSel();
                    ed.curLine = (int)ed.lines.size()-1;
                    ed.curCol  = (int)ed.lines[ed.curLine].size();
                    break;
                default: return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
        } else {
            switch (wParam) {
                case VK_UP:
                    moveSel();
                    if (ed.curLine > 0) {
                        --ed.curLine;
                        ed.curCol = std::min(ed.curCol, (int)ed.lines[ed.curLine].size());
                    }
                    break;
                case VK_DOWN:
                    moveSel();
                    if (ed.curLine + 1 < (int)ed.lines.size()) {
                        ++ed.curLine;
                        ed.curCol = std::min(ed.curCol, (int)ed.lines[ed.curLine].size());
                    }
                    break;
                case VK_LEFT:
                    if (!shift && !ed.selEmpty()) {
                        int aL = ed.selLine, aC = ed.selCol;
                        int bL = ed.curLine, bC = ed.curCol;
                        ed.selNorm(aL, aC, bL, bC);
                        ed.curLine = aL; ed.curCol = aC;
                        ed.hasSelection = false;
                    } else {
                        moveSel();
                        if (ed.curCol > 0) --ed.curCol;
                        else if (ed.curLine > 0) {
                            --ed.curLine;
                            ed.curCol = (int)ed.lines[ed.curLine].size();
                        }
                    }
                    break;
                case VK_RIGHT:
                    if (!shift && !ed.selEmpty()) {
                        int aL = ed.selLine, aC = ed.selCol;
                        int bL = ed.curLine, bC = ed.curCol;
                        ed.selNorm(aL, aC, bL, bC);
                        ed.curLine = bL; ed.curCol = bC;
                        ed.hasSelection = false;
                    } else {
                        moveSel();
                        auto& l = ed.lines[ed.curLine];
                        if (ed.curCol < (int)l.size()) ++ed.curCol;
                        else if (ed.curLine + 1 < (int)ed.lines.size()) {
                            ++ed.curLine; ed.curCol = 0;
                        }
                    }
                    break;
                case VK_HOME: {
                    moveSel();
                    auto& l = ed.lines[ed.curLine];
                    int nonWS = 0;
                    while (nonWS < (int)l.size() && (l[nonWS]==' '||l[nonWS]=='\t')) ++nonWS;
                    ed.curCol = (ed.curCol != nonWS) ? nonWS : 0;
                    break;
                }
                case VK_END:
                    moveSel();
                    ed.curCol = (int)ed.lines[ed.curLine].size();
                    break;
                case VK_PRIOR:
                    moveSel();
                    ed.curLine = std::max(0, ed.curLine - ed.visLines);
                    ed.scrollLine = std::max(0, ed.scrollLine - ed.visLines);
                    break;
                case VK_NEXT:
                    moveSel();
                    ed.curLine = std::min((int)ed.lines.size()-1, ed.curLine + ed.visLines);
                    ed.scrollLine = std::min((int)ed.lines.size()-1, ed.scrollLine + ed.visLines);
                    break;
                case VK_DELETE:
                    deleteCharFwd(ed);
                    updateTitle(hwnd);
                    break;
                default:
                    return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
        }
        ed.ensureCursorVisible();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
            case ID_FILE_NEW:
                ed.lines.clear();
                ed.lines.push_back("");
                ed.curLine = ed.curCol = 0;
                ed.scrollLine = ed.scrollCol = 0;
                ed.filePath[0] = L'\0';
                ed.modified = false;
                ed.undoStack.clear();
                ed.redoStack.clear();
                updateTitle(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;

            case ID_FILE_OPEN: {
                wchar_t buf[MAX_PATH] = {};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner   = hwnd;
                ofn.lpstrFilter = L"Assembly Files\0*.asm;*.s;*.nasm;*.inc\0All Files\0*.*\0";
                ofn.lpstrFile   = buf;
                ofn.nMaxFile    = MAX_PATH;
                ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    loadFile(ed, buf);
                    updateTitle(hwnd);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                break;
            }

            case ID_FILE_SAVE:
                if (ed.filePath[0]) {
                    saveFile(ed, ed.filePath);
                    updateTitle(hwnd);
                } else {
                    SendMessageW(hwnd, WM_COMMAND, ID_FILE_SAVEAS, 0);
                }
                break;

            case ID_FILE_SAVEAS: {
                wchar_t buf[MAX_PATH] = {};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner   = hwnd;
                ofn.lpstrFilter = L"Assembly Files\0*.asm;*.s\0All Files\0*.*\0";
                ofn.lpstrFile   = buf;
                ofn.nMaxFile    = MAX_PATH;
                ofn.lpstrDefExt = L"asm";
                ofn.Flags       = OFN_OVERWRITEPROMPT;
                if (GetSaveFileNameW(&ofn)) {
                    saveFile(ed, buf);
                    updateTitle(hwnd);
                }
                break;
            }

            case ID_FILE_EXIT:
                DestroyWindow(hwnd);
                break;

            case ID_EDIT_UNDO:  ed.undo(); updateTitle(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;
            case ID_EDIT_REDO:  ed.redo(); updateTitle(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;

            case ID_EDIT_COPY:  ed.copyToClipboard(hwnd); break;
            case ID_EDIT_CUT:
                ed.copyToClipboard(hwnd);
                ed.deleteSelection();
                updateTitle(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case ID_EDIT_PASTE:
                ed.pasteFromClipboard(hwnd);
                updateTitle(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            case ID_EDIT_SELECTALL:
                ed.selLine = 0; ed.selCol = 0;
                ed.curLine = (int)ed.lines.size()-1;
                ed.curCol  = (int)ed.lines[ed.curLine].size();
                ed.hasSelection = true;
                InvalidateRect(hwnd, nullptr, FALSE);
                break;

            case ID_VIEW_ZOOM_IN:
                g_fontSize = std::min(72, g_fontSize + 2);
                recreateFonts();
                InvalidateRect(hwnd, nullptr, FALSE);
                break;

            case ID_VIEW_ZOOM_OUT:
                g_fontSize = std::max(8, g_fontSize - 2);
                recreateFonts();
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmdLine, int nShow) {
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"AsmEditorWnd";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        WS_EX_ACCEPTFILES,
        L"AsmEditorWnd",
        L"Untitled - ASM Editor",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1100, 750,
        nullptr, nullptr, hInst, nullptr
    );

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    if (cmdLine && cmdLine[0]) {
        wchar_t path[MAX_PATH];
        wcsncpy_s(path, cmdLine, MAX_PATH);
        if (path[0] == L'"') {
            size_t len = wcslen(path);
            if (len > 1 && path[len-1] == L'"') path[len-1] = L'\0';
            loadFile(g_ed, path + 1);
        } else {
            loadFile(g_ed, path);
        }
        updateTitle(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    DragAcceptFiles(hwnd, TRUE);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
