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
    m.reserve(300);
    auto add = [&](const char* const* arr, TT t) {
        for (int i = 0; arr[i]; ++i) m[arr[i]] = t;
    };
    add(REG_KW,     TT::REGISTER);
    add(DIR_KW,     TT::DIRECTIVE);
    add(NOP_KW,     TT::NOP);
    add(STACK_KW,   TT::STACK);
    add(CALL_RET_KW,TT::CALL_RET);
    add(JUMP_KW,    TT::JUMP);
    add(MOV_LEA_KW, TT::MOV_LEA);
    // rip overrides register
    m["rip"] = TT::RIP;
    m["eip"] = TT::RIP;
    m["ip"]  = TT::RIP;
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

    // Skip whitespace (no span for whitespace)
    while (i < n) {
        // --- Whitespace ---
        if (std::isspace((unsigned char)line[i])) { ++i; continue; }

        // --- Comments: ; // # ---
        if (line[i] == ';' || (line[i] == '/' && i+1 < n && line[i+1] == '/')) {
            push(i, n - i, TT::COMMENT);
            break;
        }
        if (line[i] == '#') {
            // Check if it's a preprocessor directive or comment
            push(i, n - i, TT::COMMENT);
            break;
        }
        // Block comment start /* (multi-line not truly handled per-line but mark rest)
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
        // 0x hex, 0b bin, decimal, or numbers with suffix h/b/d/q (MASM)
        if (std::isdigit((unsigned char)line[i]) ||
            (line[i] == '-' && i+1 < n && std::isdigit((unsigned char)line[i+1])) ) {
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
                // $$ or $label
                while (i < n && isIdentCont(line[i])) ++i;
                push(start, i - start, TT::CONSTANT);
            } else {
                push(start, 1, TT::DEFAULT);
            }
            continue;
        }

        // --- Identifier / keyword / label ---
        if (isIdentStart(line[i])) {
            int start = i++;
            while (i < n && isIdentCont(line[i])) ++i;

            // Check for label: identifier followed by ':' (with optional whitespace)
            {
                int j = i;
                while (j < n && line[j] == ' ') ++j;
                if (j < n && line[j] == ':') {
                    // This whole token + colon is a label
                    push(start, j - start + 1, TT::LABEL);
                    i = j + 1;
                    continue;
                }
            }

            auto raw = line.substr(start, i - start);
            auto low = toLower(raw);

            // Section names: .text .data .bss etc.
            if (raw[0] == '.' ) {
                auto it = km.find(low);
                if (it != km.end() && it->second == TT::DIRECTIVE) {
                    // Section keywords (.text/.data/.bss) bold
                    bool isSect = (low == ".text" || low == ".data" || low == ".bss" ||
                                   low == ".rdata"|| low == ".rodata"|| low == ".code" ||
                                   low == ".const"|| low == ".tls"  || low == ".rsrc" ||
                                   low == ".reloc"|| low == ".debug"|| low == ".pdata"||
                                   low == ".xdata");
                    spans.push_back({start, i - start, isSect ? TT::SECTION : TT::DIRECTIVE, isSect});
                } else {
                    // Unknown dot-identifier: treat as directive
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
    // Lines stored as UTF-8 (or ANSI – we keep it narrow internally)
    std::vector<std::string> lines;

    // Cursor position
    int   curLine   = 0;
    int   curCol    = 0;    // byte offset within line

    // Selection: anchor
    int   selLine   = 0;
    int   selCol    = 0;
    bool  hasSelection = false;

    // Scroll offset
    int   scrollLine = 0;
    int   scrollCol  = 0;   // in characters

    // Metrics (set during paint)
    int   charW  = 0;
    int   charH  = 0;
    int   lineNumW = 0;    // width of line-number gutter in pixels
    int   visLines = 0;
    int   visChars = 0;

    // Window handle
    HWND  hwnd = nullptr;

    // Font
    HFONT font     = nullptr;
    HFONT fontBold = nullptr;

    // File path
    wchar_t filePath[MAX_PATH] = {};

    // Modified flag
    bool modified = false;

    // Undo/Redo stack  (simple snapshot of full text – good enough for moderate files)
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

    // Convert (line, col) to pixel rect relative to client
    RECT caretRect(int ln, int col) const {
        RECT r;
        r.top    = (ln - scrollLine) * charH;
        r.bottom = r.top + charH;
        r.left   = lineNumW + (col - scrollCol) * charW;
        r.right  = r.left + 2;
        return r;
    }

    // Line & col from pixel (x,y) relative to client
    void posFromPoint(int x, int y, int& ln, int& col) const {
        ln = y / charH + scrollLine;
        ln = std::clamp(ln, 0, (int)lines.size()-1);
        col = (x - lineNumW) / charW + scrollCol;
        col = std::clamp(col, 0, (int)lines[ln].size());
    }

    // -----------------------------------------------------------------------
    // Selection helpers
    // -----------------------------------------------------------------------

    // Normalise so (aL,aC) <= (bL,bC)
    void selNorm(int& aL, int& aC, int& bL, int& bC) const {
        if (aL > bL || (aL == bL && aC > bC)) {
            std::swap(aL, bL);
            std::swap(aC, bC);
        }
    }

    bool selEmpty() const {
        return !hasSelection || (curLine == selLine && curCol == selCol);
    }

    // Collect selected text as a single string (lines joined with \n)
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

    // Delete selected region; cursor moves to start of selection
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

    // Set anchor to current cursor (begin a new selection)
    void selAnchor() {
        selLine = curLine;
        selCol  = curCol;
        hasSelection = true;
    }

    // Copy to Windows clipboard
    void copyToClipboard(HWND hwnd) const {
        if (selEmpty()) return;
        std::string txt = selText();
        // Convert \n -> \r\n for Windows clipboard
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

    // Paste from Windows clipboard at cursor (replacing selection if any)
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
        L"Cascadia Mono"   // falls back to Consolas if not installed
    );
}

// If Cascadia Mono is unavailable, retry with Consolas
static HFONT makeFont2(int height, bool bold) {
    HFONT f = makeFont(height, bold);
    // Check if we actually got the right face
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

    // Vertical
    si.nMin  = 0;
    si.nMax  = (int)g_ed.lines.size() - 1;
    si.nPage = g_ed.visLines;
    si.nPos  = g_ed.scrollLine;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    // Horizontal – find max line length
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

    // --- Double-buffer ---
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, client.right, client.bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    SetBkMode(memDC, TRANSPARENT);

    // Background
    HBRUSH bgBrush = CreateSolidBrush(Col::BG);
    FillRect(memDC, &client, bgBrush);
    DeleteObject(bgBrush);

    // Measure char dimensions once
    SelectObject(memDC, g_ed.font);
    TEXTMETRICW tm{};
    GetTextMetricsW(memDC, &tm);
    g_ed.charW = tm.tmAveCharWidth;
    g_ed.charH = tm.tmHeight + tm.tmExternalLeading;
    g_ed.visLines = (client.bottom / g_ed.charH) + 1;
    g_ed.visChars = ((client.right - g_ed.lineNumW) / g_ed.charW) + 1;

    // Line number gutter width
    int numDigits = 1;
    for (int x = (int)g_ed.lines.size(); x >= 10; x /= 10) ++numDigits;
    g_ed.lineNumW = (numDigits + 2) * g_ed.charW;

    // Draw gutter background
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
            // Full-line selection: extend highlight to end of visible area
            int xStart = g_ed.lineNumW + (cStart - g_ed.scrollCol) * g_ed.charW;
            int xEnd   = (li < bL)
                ? client.right   // full line selected
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

    // Render lines
    int firstLine = g_ed.scrollLine;
    int lastLine  = std::min((int)g_ed.lines.size()-1, firstLine + g_ed.visLines + 1);

    for (int li = firstLine; li <= lastLine; ++li) {
        int yPx = (li - g_ed.scrollLine) * g_ed.charH;
        const std::string& lineStr = g_ed.lines[li];

        // --- Line number ---
        {
            char buf[16];
            int nc = snprintf(buf, sizeof(buf), "%*d", numDigits, li + 1);
            SetTextColor(memDC, Col::LINE_NUM_FG);
            SelectObject(memDC, g_ed.font);
            TextOutA(memDC, g_ed.charW/2, yPx, buf, nc);
        }

        if (lineStr.empty()) continue;

        // --- Syntax spans ---
        auto spans = tokenizeLine(lineStr);

        // We'll render char by char within spans for simplicity & scroll correctness
        // First build a per-char type array
        std::vector<TT>   types(lineStr.size(), TT::DEFAULT);
        std::vector<bool> bolds(lineStr.size(), false);
        for (auto& sp : spans) {
            int end = std::min(sp.col + sp.len, (int)lineStr.size());
            for (int ci = sp.col; ci < end; ++ci) {
                types[ci] = sp.type;
                bolds[ci] = sp.bold;
            }
        }

        // Render runs of same type (for performance)
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

    // Draw caret
    {
        RECT cr = g_ed.caretRect(g_ed.curLine, g_ed.curCol);
        HBRUSH caretBrush = CreateSolidBrush(Col::CARET);
        RECT caretFill = {cr.left, cr.top, cr.left+2, cr.bottom};
        FillRect(memDC, &caretFill, caretBrush);
        DeleteObject(caretBrush);
    }

    // Blit
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

    // Auto-indent: copy leading whitespace from current line
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
    if (sz.QuadPart > 64*1024*1024) { CloseHandle(h); return; } // 64 MB limit

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

    // Strip \r
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
    // Extract filename
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
        ed.lines.push_back("");  // start with one empty line
        recreateFonts();
        SetMenu(hwnd, buildMenu());

        // Enable caret
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

        // Update caret position
        RECT cr = ed.caretRect(ed.curLine, ed.curCol);
        SetCaretPos(cr.left, cr.top);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si{sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wParam)) {
            case SB_LINEUP:       --ed.scrollLine; break;
            case SB_LINEDOWN:     ++ed.scrollLine; break;
            case SB_PAGEUP:       ed.scrollLine -= ed.visLines; break;
            case SB_PAGEDOWN:     ed.scrollLine += ed.visLines; break;
            case SB_THUMBTRACK:   ed.scrollLine = si.nTrackPos; break;
            case SB_TOP:          ed.scrollLine = 0; break;
            case SB_BOTTOM:       ed.scrollLine = (int)ed.lines.size()-1; break;
        }
        ed.scrollLine = std::clamp(ed.scrollLine, 0, (int)ed.lines.size()-1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_HSCROLL: {
        SCROLLINFO si{sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd, SB_HORZ, &si);
        switch (LOWORD(wParam)) {
            case SB_LINELEFT:  --ed.scrollCol; break;
            case SB_LINERIGHT: ++ed.scrollCol; break;
            case SB_PAGELEFT:  ed.scrollCol -= ed.visChars; break;
            case SB_PAGERIGHT: ed.scrollCol += ed.visChars; break;
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
            // Fresh click: set anchor = cursor
            ed.posFromPoint(x, y, ed.curLine, ed.curCol);
            ed.selAnchor();
            ed.hasSelection = false;
        } else {
            // Shift+click: extend selection from existing anchor
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
        // Collapse zero-length selection
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

        // Helper: begin/extend selection when shift held, clear when not
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
                    // Select all
                    ed.selLine = 0; ed.selCol = 0;
                    ed.curLine = (int)ed.lines.size()-1;
                    ed.curCol  = (int)ed.lines[ed.curLine].size();
                    ed.hasSelection = true;
                    break;
                }
                case 'C':
                    ed.copyToClipboard(hwnd);
                    return 0;
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
                        // Jump to selection start
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
                        // Jump to selection end
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
                case VK_PRIOR: // Page Up
                    moveSel();
                    ed.curLine = std::max(0, ed.curLine - ed.visLines);
                    ed.scrollLine = std::max(0, ed.scrollLine - ed.visLines);
                    break;
                case VK_NEXT:  // Page Down
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

            case ID_EDIT_UNDO: ed.undo(); updateTitle(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;
            case ID_EDIT_REDO: ed.redo(); updateTitle(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;

            case ID_EDIT_COPY:
                ed.copyToClipboard(hwnd);
                break;
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
        return 1; // We handle background in WM_PAINT

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmdLine, int nShow) {
    // DPI awareness
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    // Register window class
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_IBEAM);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); // overridden by WM_ERASEBKGND
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

    // Open file from command line if provided
    if (cmdLine && cmdLine[0]) {
        wchar_t path[MAX_PATH];
        wcsncpy_s(path, cmdLine, MAX_PATH);
        // Strip surrounding quotes if any
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

    // Add a drag-drop handler via WM_DROPFILES
    DragAcceptFiles(hwnd, TRUE);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
