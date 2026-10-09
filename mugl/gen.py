#!/usr/bin/env python3
import re
import sys

HEADER, OUT = sys.argv[1], sys.argv[2]

MANUAL_CLIENT = {
    "glBindBuffer", "glPixelStorei", "glEnableVertexAttribArray", "glDisableVertexAttribArray",
    "glFinish", "glGetError", "glDrawArrays",
}

SCALAR = {"GLenum", "GLuint", "GLint", "GLsizei", "GLboolean", "GLbitfield", "GLfloat", "GLclampf"}
FLOATS = {"GLfloat", "GLclampf"}

proto = re.compile(r"^GL_APICALL\s+(.+?)\s*GL_APIENTRY\s+(gl\w+)\s*\((.*)\);")
funcs = []
for line in open(HEADER):
    m = proto.match(line.strip())
    if not m:
        continue
    ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
    params = []
    if args != "void":
        for a in args.split(","):
            a = a.strip()
            pm = re.match(r"(.*?)(\w+)$", a)
            params.append((pm.group(1).strip(), pm.group(2)))
    funcs.append((ret, name, params))

simple = []
for ret, name, params in funcs:
    if "*" in ret or any("*" in t for t, _ in params):
        continue
    if any(t.replace("const", "").strip() not in SCALAR for t, _ in params):
        continue
    if ret not in ("void", "GLenum", "GLuint", "GLboolean"):
        continue
    simple.append((ret, name, params))

ops = open(f"{OUT}/gen_ops.h", "w")
ops.write("#pragma once\n\n#include \"proto.h\"\n\n")
for i, (_, name, _) in enumerate(funcs):
    ops.write(f"#define MUGL_OP_{name} (MUGL_OP_GENERATED_BASE + {i}U)\n")
ops.write(f"\n#define MUGL_GENERATED_COUNT {len(funcs)}U\n")
ops.close()


def encode(params):
    lines = []
    for i, (t, n) in enumerate(params):
        if t in FLOATS:
            lines.append(f"    memcpy(&a[{i}], &{n}, 4);")
        else:
            lines.append(f"    a[{i}] = (uint32_t) {n};")
    return lines


cl = open(f"{OUT}/client_gen.c", "w")
cl.write("#include <string.h>\n#include <stdint.h>\n#include <GLES2/gl2.h>\n#include \"client.h\"\n#include \"gen_ops.h\"\n\n")
for ret, name, params in simple:
    if name in MANUAL_CLIENT:
        continue
    sig = ", ".join(f"{t} {n}" for t, n in params) or "void"
    cl.write(f"GL_APICALL {ret} GL_APIENTRY {name}({sig}) {{\n")
    count = len(params)
    if count:
        cl.write(f"    uint32_t a[{count}];\n")
        cl.write("\n".join(encode(params)) + "\n")
    arg_ptr = "a" if count else "NULL"
    if ret == "void":
        cl.write(f"    mugl_send(MUGL_OP_{name}, {arg_ptr}, {count});\n")
    else:
        cl.write(f"    return ({ret}) mugl_call_u32(MUGL_OP_{name}, {arg_ptr}, {count});\n")
    cl.write("}\n\n")
cl.write("const mugl_proc mugl_gl_procs[] = {\n")
for ret, name, params in funcs:
    cl.write(f"    {{\"{name}\", (void (*)(void)) {name}}},\n")
cl.write("};\n\n")
cl.write(f"const unsigned mugl_gl_proc_count = {len(funcs)}U;\n")
cl.close()

sv = open(f"{OUT}/server_gen.c", "w")
sv.write("#include <string.h>\n#include <stdint.h>\n#include <SDL2/SDL.h>\n#include <GLES2/gl2.h>\n#include \"server.h\"\n#include \"gen_ops.h\"\n\n")
for ret, name, params in funcs:
    sv.write(f"__typeof__({name}) *p_{name};\n")
sv.write("\nint server_load_gl(void) {\n")
for ret, name, params in funcs:
    sv.write(f"    if (!(p_{name} = (__typeof__({name}) *) SDL_GL_GetProcAddress(\"{name}\"))) return server_missing(\"{name}\");\n")
sv.write("    return 1;\n}\n\n")


def decode(params):
    out = []
    for i, (t, n) in enumerate(params):
        if t in FLOATS:
            out.append(f"server_f(a[{i}])")
        else:
            out.append(f"({t}) a[{i}]")
    return ", ".join(out)


sv.write("int server_dispatch_generated(uint32_t op, const uint32_t *a, uint32_t words) {\n")
sv.write("    switch (op) {\n")
for ret, name, params in simple:
    sv.write(f"        case MUGL_OP_{name}:\n")
    if params:
        sv.write(f"            if (words < {len(params)}U) return -1;\n")
    if ret == "void":
        sv.write(f"            p_{name}({decode(params)});\n")
    else:
        sv.write(f"            server_reply_u32((uint32_t) p_{name}({decode(params)}));\n")
    sv.write("            return 1;\n")
sv.write("        default:\n            return 0;\n    }\n}\n")
sv.close()

print(f"{len(funcs)} functions, {len(simple)} generated, {len(funcs) - len(simple)} pointer functions")
