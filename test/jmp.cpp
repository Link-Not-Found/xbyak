#include <stdio.h>
#include <string.h>
#include <string>
#define XBYAK_NO_OP_NAMES
#include <xbyak/xbyak.h>
#include <cybozu/inttype.hpp>
#include <cybozu/test.hpp>

using namespace Xbyak;

void putNop(Xbyak::CodeGenerator *gen, int n)
{
	for (int i = 0; i < n; i++) {
		gen->nop();
	}
}

void diff(const std::string& a, const std::string& b)
{
	if (a == b) return;
	if (a.size() != b.size()) printf("size diff %d %d\n", (int)a.size(), (int)b.size());
	for (size_t i = 0; i < (std::min)(a.size(), b.size()); i++) {
		if (a[i] != b[i]) {
			printf("diff %d(%04x) %02x %02x\n", (int)i, (int)i, (unsigned char)a[i], (unsigned char)b[i]);
		}
	}
}

void dump(const std::string& m)
{
	printf("size=%d\n     ", (int)m.size());
	for (int i = 0; i < 16; i++) {
		printf("%02x ", i);
	}
	printf("\n     ");
	for (int i = 0; i < 16; i++) {
		printf("---");
	}
	printf("\n");
	for (size_t i = 0; i < m.size(); i++) {
		if ((i % 16) == 0) printf("%04x ", (int)(i / 16));
		printf("%02x ", (unsigned char)m[i]);
		if ((i % 16) == 15) putchar('\n');
	}
	putchar('\n');
}

CYBOZU_TEST_AUTO(test1)
{
	struct TestJmp : public Xbyak::CodeGenerator {
	/*
	     4                                  X0:
	     5 00000004 EBFE                    jmp short X0
	     6
	     7                                  X1:
	     8 00000006 <res 00000001>          dummyX1 resb 1
	     9 00000007 EBFD                    jmp short X1
	    10
	    11                                  X126:
	    12 00000009 <res 0000007E>          dummyX126 resb 126
	    13 00000087 EB80                    jmp short X126
	    14
	    15                                  X127:
	    16 00000089 <res 0000007F>          dummyX127 resb 127
	    17 00000108 E97CFFFFFF              jmp near X127
	    18
	    19 0000010D EB00                    jmp short Y0
	    20                                  Y0:
	    21
	    22 0000010F EB01                    jmp short Y1
	    23 00000111 <res 00000001>          dummyY1 resb 1
	    24                                  Y1:
	    25
	    26 00000112 EB7F                    jmp short Y127
	    27 00000114 <res 0000007F>          dummyY127 resb 127
	    28                                  Y127:
	    29
	    30 00000193 E980000000              jmp near Y128
	    31 00000198 <res 00000080>          dummyY128 resb 128
	    32                                  Y128:
	*/
		TestJmp(int offset, bool isBack, bool isShort, bool useNewLabel)
		{
			if (useNewLabel) {
				Label label;
				if (isBack) {
					L(label);
					putNop(this, offset);
					jmp(label);
				} else {
					if (isShort) {
						jmp(label);
					} else {
						jmp(label, T_NEAR);
					}
					putNop(this, offset);
					L(label);
				}
			} else {
				if (isBack) {
					L("@@");
					putNop(this, offset);
					jmp("@b");
				} else {
					if (isShort) {
						jmp("@f");
					} else {
						jmp("@f", T_NEAR);
					}
					putNop(this, offset);
					L("@@");
				}
			}
		}
	};
	static const struct Tbl {
		int offset;
		bool isBack;
		bool isShort;
		uint8_t result[6];
		int size;
	} tbl[] = {
		{ 0, true, true, { 0xeb, 0xfe }, 2 },
		{ 1, true, true, { 0xeb, 0xfd }, 2 },
		{ 126, true, true, { 0xeb, 0x80 }, 2 },
		{ 127, true, false, {0xe9, 0x7c, 0xff, 0xff, 0xff }, 5 },
		{ 0, false, true, { 0xeb, 0x00 }, 2 },
		{ 1, false, true, { 0xeb, 0x01 }, 2 },
		{ 127, false, true, { 0xeb, 0x7f }, 2 },
		{ 128, false, false, { 0xe9, 0x80, 0x00, 0x00, 0x00 }, 5 },
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		const Tbl *p = &tbl[i];
		for (int k = 0; k < 2; k++) {
			TestJmp jmp(p->offset, p->isBack, p->isShort, k == 0);
			const uint8_t *q = (const uint8_t*)jmp.getCode();
			if (p->isBack) q += p->offset; /* skip nop */
			for (int j = 0; j < p->size; j++) {
				CYBOZU_TEST_EQUAL(q[j], p->result[j]);
			}
		}
	}
}

CYBOZU_TEST_AUTO(testJmpCx)
{
	struct TestJmpCx : public CodeGenerator {
		explicit TestJmpCx(void *p, bool useNewLabel)
			: Xbyak::CodeGenerator(16, p)
		{
			if (useNewLabel) {
				Label lp;
			L(lp);
#ifdef XBYAK64
				/*
					67 E3 FD ; jecxz lp
					E3 FB    ; jrcxz lp
				*/
				jecxz(lp);
				jrcxz(lp);
#else
				/*
					E3FE   ; jecxz lp
					67E3FB ; jcxz lp
				*/
				jecxz(lp);
				jcxz(lp);
#endif
			} else {
				inLocalLabel();
			L(".lp");
#ifdef XBYAK64
				/*
					67 E3 FD ; jecxz lp
					E3 FB    ; jrcxz lp
				*/
				jecxz(".lp");
				jrcxz(".lp");
#else
				/*
					E3FE   ; jecxz lp
					67E3FB ; jcxz lp
				*/
				jecxz(".lp");
				jcxz(".lp");
#endif
				outLocalLabel();
			}
		}
	};
	const struct {
		const char *p;
		size_t len;
	} tbl = {
#ifdef XBYAK64
		"\x67\xe3\xfd\xe3\xfb", 5
#else
		"\xe3\xfe\x67\xe3\xfb", 5
#endif
	};
	for (int j = 0; j < 2; j++) {
		char buf[16] = {};
		TestJmpCx code(buf, j == 0);
		CYBOZU_TEST_EQUAL(memcmp(buf, tbl.p, tbl.len), 0);
	}
}

CYBOZU_TEST_AUTO(loop)
{
	const uint8_t ok[] = {
		// lp:
		0x31, 0xC0, // xor eax, eax
		0xE2, 0xFC, // loop lp
		0xE0, 0xFA, // loopne lp
		0xE1, 0xF8, // loope lp
	};
	struct Code : CodeGenerator {
		Code(bool useLabel)
		{
			if (useLabel) {
				Xbyak::Label lp = L();
				xor_(eax, eax);
				loop(lp);
				loopne(lp);
				loope(lp);
			} else {
				L("@@");
				xor_(eax, eax);
				loop("@b");
				loopne("@b");
				loope("@b");
			}
		}
	};
	Code code1(false);
	CYBOZU_TEST_EQUAL(code1.getSize(), sizeof(ok));
	CYBOZU_TEST_EQUAL_ARRAY(code1.getCode(), ok, sizeof(ok));
	Code code2(true);
	CYBOZU_TEST_EQUAL(code2.getSize(), sizeof(ok));
	CYBOZU_TEST_EQUAL_ARRAY(code2.getCode(), ok, sizeof(ok));
}

#ifdef _MSC_VER
	#pragma warning(disable : 4310)
#endif
CYBOZU_TEST_AUTO(test2)
{
	struct TestJmp2 : public CodeGenerator {
	/*
	  1 00000000 90                      nop
	  2 00000001 90                      nop
	  3                                  f1:
	  4 00000002 <res 0000007E>          dummyX1 resb 126
	  6 00000080 EB80                     jmp f1
	  7
	  8                                  f2:
	  9 00000082 <res 0000007F>          dummyX2 resb 127
	 11 00000101 E97CFFFFFF               jmp f2
	 12
	 13
	 14 00000106 EB7F                    jmp f3
	 15 00000108 <res 0000007F>          dummyX3 resb 127
	 17                                  f3:
	 18
	 19 00000187 E980000000              jmp f4
	 20 0000018C <res 00000080>          dummyX4 resb 128
	 22                                  f4:
	*/
		TestJmp2(void *p, bool useNewLabel)
			: Xbyak::CodeGenerator(8192, p)
		{
			if (useNewLabel) {
				inLocalLabel();
				nop();
				nop();
			L(".f1");
				putNop(this, 126);
				jmp(".f1");
			L(".f2");
				putNop(this, 127);
				jmp(".f2", T_NEAR);

				jmp(".f3");
				putNop(this, 127);
			L(".f3");
				jmp(".f4", T_NEAR);
				putNop(this, 128);
			L(".f4");
				outLocalLabel();
			} else {
				nop();
				nop();
				Label f1, f2, f3, f4;
			L(f1);
				putNop(this, 126);
				jmp(f1);
			L(f2);
				putNop(this, 127);
				jmp(f2, T_NEAR);

				jmp(f3);
				putNop(this, 127);
			L(f3);
				jmp(f4, T_NEAR);
				putNop(this, 128);
			L(f4);
			}
		}
	};

	std::string ok;
	ok.resize(0x18C + 128, (char)0x90);
	ok[0x080] = (char)0xeb;
	ok[0x081] = (char)0x80;

	ok[0x101] = (char)0xe9;
	ok[0x102] = (char)0x7c;
	ok[0x103] = (char)0xff;
	ok[0x104] = (char)0xff;
	ok[0x105] = (char)0xff;

	ok[0x106] = (char)0xeb;
	ok[0x107] = (char)0x7f;

	ok[0x187] = (char)0xe9;
	ok[0x188] = (char)0x80;
	ok[0x189] = (char)0x00;
	ok[0x18a] = (char)0x00;
	ok[0x18b] = (char)0x00;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			TestJmp2 c(i == 0 ? 0 : Xbyak::AutoGrow, j == 0);
			c.ready();
			std::string m((const char*)c.getCode(), c.getSize());
			CYBOZU_TEST_EQUAL(m, ok);
		}
	}
}

#ifdef XBYAK32
int add5(int x) { return x + 5; }
int add2(int x) { return x + 2; }

CYBOZU_TEST_AUTO(test3)
{
	struct Grow : Xbyak::CodeGenerator {
		Grow(int dummySize)
			: Xbyak::CodeGenerator(128, Xbyak::AutoGrow)
		{
			mov(eax, 100);
			push(eax);
			call((void*)add5);
			add(esp, 4);
			push(eax);
			call((void*)add2);
			add(esp, 4);
			ret();
			for (int i = 0; i < dummySize; i++) {
				db(0);
			}
		}
	};
	for (int dummySize = 0; dummySize < 40000; dummySize += 10000) {
		printf("dummySize=%d\n", dummySize);
		Grow g(dummySize);
		g.ready();
		int (*f)() = g.getCode<int (*)()>();
		int x = f();
		const int ok = 107;
		CYBOZU_TEST_EQUAL(x, ok);
	}
}

CYBOZU_TEST_AUTO(addr_label_backward_ref1)
{
	const int c1 = 123;
	const int c2 = 567;
	const int c3 = 999;
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Label L1, L2;
			jmp(L2);
		L(L1);
			dd(c1);
			dd(c2);
		L(L2);
			mov(eax, ptr[L1]);
			add(dword[L1+4], c3);
			add(eax, ptr[L1+4]);
			ret();
		}
	} code;
	int v = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(v, c1 + c2 + c3);
}

CYBOZU_TEST_AUTO(addr_label_backward_ref2)
{
	const int c = 123;
	const int N = 3;

	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Label L1, L2;
			jmp(L2);
		L(L1);
			for (int i = 0; i < N; i++) {
				dd(c + i);
			}
		L(L2);
			xor_(ecx, ecx);
			mov(eax, ptr[L1+ecx]);
			for (int i = 1; i < N; i++) {
				add(eax, ptr[L1+ecx+i*4]);
			}
			ret();
		}
	} code;
	int v = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(v, c * N + N * (N-1)/2);
}

CYBOZU_TEST_AUTO(addr_label_forward_ref1)
{
	using namespace Xbyak;
	static const int c1 = 10;
	static const int c2 = 100;
	static const int c3 = 1000;
	static const int c4 = 10000;
	static const int c5 = 100000;
	struct Code : CodeGenerator {
		Code(size_t size, void *mode)
			: CodeGenerator(size, mode)
		{
			Label L1, L2, L3;
			mov(eax, ptr[L1]); // c1
			mov(ecx, ptr[L1+4]); // c2
			add(eax, ecx);
			add(eax, ptr[L2]); // c2
			add(eax, ptr[L2+4]); // c3
			call(L3);
//			call(L3 + 32);
			ret();
			for (int i = 0; i < 4096; i++) {
				db(0);
			}
		L(L1);
			dd(c1);
		L(L2);
			dd(c2);
			dd(c3);
			align(32);
		L(L3);
			add(eax, c4);
			ret();
			align(32);
//		L(L4);
			add(eax, c5);
			ret();

			ready();
		}
		void test()
		{
			int v = getCode<int (*)()>()();
			CYBOZU_TEST_EQUAL(v, c1 + c2 * 2 + c3 + c4 /*+ c5*/);
		}
	};
	Code code1(8096, 0);
	code1.test();
	Code code2(4096, Xbyak::AutoGrow);
	code2.test();
}

#endif

uint8_t bufL[4096 * 32];
uint8_t bufS[4096 * 2];

struct MyAllocator : Xbyak::Allocator {
	uint8_t *alloc(size_t size)
	{
		if (size < sizeof(bufS)) {
			printf("test use bufS(%d)\n", (int)size);
			return bufS;
		}
		if (size < sizeof(bufL)) {
			printf("test use bufL(%d)\n", (int)size);
			return bufL;
		}
		fprintf(stderr, "no memory %d\n", (int)size);
		exit(1);
	}
	void free(uint8_t *)
	{
	}
} myAlloc;

CYBOZU_TEST_AUTO(test4)
{
	struct Test4 : Xbyak::CodeGenerator {
		Test4(int size, void *mode, bool useNewLabel)
			: CodeGenerator(size, mode)
		{
			if (useNewLabel) {
				Label x;
				jmp(x);
				putNop(this, 10);
			L(x);
				ret();
			} else {
				inLocalLabel();
				jmp(".x");
				putNop(this, 10);
			L(".x");
				ret();
				outLocalLabel();
			}
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool useNewLabel = i == 0;
		std::string fm, gm;
		Test4 fc(1024, 0, useNewLabel);
		Test4 gc(5, Xbyak::AutoGrow, !useNewLabel);
		gc.ready();
		fm.assign((const char*)fc.getCode(), fc.getSize());
		gm.assign((const char*)gc.getCode(), gc.getSize());
		CYBOZU_TEST_EQUAL(fm, gm);
	}
}

#ifndef __APPLE__
CYBOZU_TEST_AUTO(test5)
{
	struct Test5 : Xbyak::CodeGenerator {
		explicit Test5(int size, int count, void *mode)
			: CodeGenerator(size, mode, &myAlloc)
		{
			using namespace Xbyak;
			inLocalLabel();
			mov(ecx, count);
			xor_(eax, eax);
		L(".lp");
			for (int i = 0; i < count; i++) {
				L(Label::toStr(i));
				add(eax, 1);
				int to = 0;
				if (i < count / 2) {
					to = count - 1 - i;
				} else {
					to = count  - i;
				}
				if (i == count / 2) {
					jmp(".exit", T_NEAR);
				} else {
					jmp(Label::toStr(to), T_NEAR);
				}
			}
		L(".exit");
			sub(ecx, 1);
			jnz(".lp", T_NEAR);
			ret();
			outLocalLabel();
		}
	};
	std::string fm, gm;
	const int count = 50;
	int ret;
	Test5 fc(1024 * 64, count, 0);
	ret = fc.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, count * count);
	fm.assign((const char*)fc.getCode(), fc.getSize());
	Test5 gc(10, count, Xbyak::AutoGrow);
	gc.ready();
	ret = gc.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, count * count);
	gm.assign((const char*)gc.getCode(), gc.getSize());
	CYBOZU_TEST_EQUAL(fm, gm);
}
#endif

size_t getValue(const uint8_t* p)
{
	size_t v = 0;
	for (size_t i = 0; i < sizeof(size_t); i++) {
		v |= size_t(p[i]) << (i * 8);
	}
	return v;
}

void checkAddr(const uint8_t *p, size_t offset, size_t expect)
{
	size_t v = getValue(p + offset);
	CYBOZU_TEST_EQUAL(v, size_t(p) + expect);
}

CYBOZU_TEST_AUTO(MovLabel)
{
	struct MovLabelCode : Xbyak::CodeGenerator {
		MovLabelCode(bool grow, bool useNewLabel)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
#ifdef XBYAK64
			const Reg64& a = rax;
#else
			const Reg32& a = eax;
#endif
			if (useNewLabel) {
				nop(); // 0x90
				Label lp1, lp2;
			L(lp1);
				nop();
				mov(a, lp1); // 0xb8 + <4byte> / 0x48bb + <8byte>
				nop();
				mov(a, lp2); // 0xb8
				// force realloc if AutoGrow
				putNop(this, 256);
				nop();
			L(lp2);
			} else {
				inLocalLabel();
				nop(); // 0x90
			L(".lp1");
				nop();
				mov(a, ".lp1"); // 0xb8 + <4byte> / 0x48bb + <8byte>
				nop();
				mov(a, ".lp2"); // 0xb8
				// force realloc if AutoGrow
				putNop(this, 256);
				nop();
			L(".lp2");
				outLocalLabel();
			}
		}
	};

	const struct {
		int pos;
		uint8_t ok;
	} tbl[] = {
#ifdef XBYAK32
		{ 0x00, 0x90 },
		// lp1:0x001
		{ 0x001, 0x90 },
		{ 0x002, 0xb8 },
		// 0x003
		{ 0x007, 0x90 },
		{ 0x008, 0xb8 },
		// 0x009
		{ 0x10d, 0x90 },
		// lp2:0x10e
#else
		{ 0x000, 0x90 },
		// lp1:0x001
		{ 0x001, 0x90 },
		{ 0x002, 0x48 },
		{ 0x003, 0xb8 },
		// 0x004
		{ 0x00c, 0x90 },
		{ 0x00d, 0x48 },
		{ 0x00e, 0xb8 },
		// 0x00f
		{ 0x117, 0x90 },
		// lp2:0x118
#endif
	};
	for (int j = 0; j < 2; j++) {
		const bool grow = j == 0;
		for (int k = 0; k < 2; k++) {
			const bool useNewLabel = k == 0;
			MovLabelCode code(grow, useNewLabel);
			if (grow) code.ready();
			const uint8_t* const p = code.getCode();
			for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
				int pos = tbl[i].pos;
				uint8_t x = p[pos];
				uint8_t ok = tbl[i].ok;
				CYBOZU_TEST_EQUAL(x, ok);
			}
#ifdef XBYAK32
			checkAddr(p, 0x03, 0x001);
			checkAddr(p, 0x09, 0x10e);
#else
			checkAddr(p, 0x04, 0x001);
			checkAddr(p, 0x0f, 0x118);
#endif
		}
	}
}

CYBOZU_TEST_AUTO(testMovLabel2)
{
	struct MovLabel2Code : Xbyak::CodeGenerator {
		MovLabel2Code()
		{
#ifdef XBYAK64
			const Reg64& a = rax;
			const Reg64& c = rcx;
#else
			const Reg32& a = eax;
			const Reg32& c = ecx;
#endif
			xor_(a, a);
			xor_(c, c);
			jmp("in");
			ud2();
		L("@@"); // L1
			add(a, 2);
			mov(c, "@f");
			jmp(c); // goto L2
			ud2();
		L("in");
			mov(c, "@b");
			add(a, 1);
			jmp(c); // goto L1
			ud2();
		L("@@"); // L2
			add(a, 4);
			ret();
		}
	};
	MovLabel2Code code;
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, 7);
}

CYBOZU_TEST_AUTO(testF_B)
{
	struct Code : Xbyak::CodeGenerator {
		Code(int type)
		{
			inLocalLabel();
			xor_(eax, eax);
			switch (type) {
			case 0:
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 1:
				test(eax, eax);
				jz("@f");
				ud2();
			L("@@");
				break;
			case 2:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				break;
			case 3:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				jmp("@f");
				ud2();
			L("@@");
				break;
			case 4:
			L("@@");
				inc(eax);
				cmp(eax, 1); // 1, 2
				je("@b");
				cmp(eax, 2); // 2, 3
				je("@b");
				jmp("@f");
				ud2();
			L("@@");
				inc(eax); // 4, 5
				cmp(eax, 4);
				je("@b");
				break;
			case 5:
			L("@@");
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 6:
			L("@@");
			L("@@");
			L("@@");
				inc(eax);
				cmp(eax, 1);
				je("@b");
				break;
			case 7:
				jmp("@f");
			L("@@");
				inc(eax); // 1, 2
				cmp(eax, 1);
				je("@b");
				cmp(eax, 2);
				jne("@f"); // not jmp
				inc(eax); // 3
			L("@@");
				inc(eax); // 4, 5, 6
				cmp(eax, 4);
				je("@b");
				cmp(eax, 5);
				je("@b");
				jmp("@f");
				jmp("@f");
				jmp("@b");
			L("@@");
				break;
			}
			ret();
			outLocalLabel();
		}
	};
	const int expectedTbl[] = {
		2, 0, 3, 3, 5, 2, 2, 6
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(expectedTbl); i++) {
		Code code((int)i);
		int ret = code.getCode<int (*)()>()();
		CYBOZU_TEST_EQUAL(ret, expectedTbl[i]);
	}
}

CYBOZU_TEST_AUTO(test6)
{
	struct TestLocal : public Xbyak::CodeGenerator {
		TestLocal(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			inLocalLabel();
			jmp("start0", T_NEAR);
			L(".back");
			inc(eax); // 8
			jmp(".next", T_NEAR);
			L("start2");
			inc(eax); // 7
			jmp(".back", T_NEAR);
				inLocalLabel();
				L(".back");
				inc(eax); // 5
				putNop(this, 128);
				jmp(".next", T_NEAR);
				L("start1");
				inc(eax); // 4
				jmp(".back", T_NEAR);
					inLocalLabel();
					L(".back");
					inc(eax); // 2
					jmp(".next", T_NEAR);
					L("start0");
					inc(eax); // 1
					jmp(".back", T_NEAR);
					L(".next");
					inc(eax); // 3
					jmp("start1", T_NEAR);
					outLocalLabel();
				L(".next");
				inc(eax); // 6
				jmp("start2", T_NEAR);
				outLocalLabel();
			L(".next");
			inc(eax); // 9
			jmp("start3", T_NEAR);
				inLocalLabel();
				L(".back");
				inc(eax); // 14
				jmp("exit", T_NEAR);
			L("start4");
				inc(eax); // 13
				jmp(".back", T_NEAR);
				outLocalLabel();
			L("start3");
				inc(eax); // 10
				inLocalLabel();
				jmp(".next", T_NEAR);
				L(".back");
				inc(eax); // 12
				jmp("start4", T_NEAR);
				L(".next");
				inc(eax); // 11
				jmp(".back", T_NEAR);
				outLocalLabel();
			outLocalLabel();
			L("exit");
			inc(eax); // 15
			ret();
		}
	};

	for (int i = 0; i < 2; i++) {
		const bool grow = i == 1;
		printf("test6 grow=%d\n", i);
		TestLocal code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int a = f();
		CYBOZU_TEST_EQUAL(a, 15);
	}
}

CYBOZU_TEST_AUTO(test_jcc)
{
	struct A : Xbyak::CodeGenerator {
		A()
		{
			add(eax, 5);
			ret();
		}
	};
	struct B : Xbyak::CodeGenerator {
		B(bool grow, const void *p) : Xbyak::CodeGenerator(grow ? 0 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			mov(eax, 1);
			add(eax, 2);
			jnz(p);
		}
	};
	A a;
	const void *p = a.getCode<const void*>();
	for (int i = 0; i < 2; i++) {
		bool grow = i == 1;
		B b(grow, p);
		if (grow) {
			b.ready();
		}
		int (*f)() = b.getCode<int (*)()>();
		CYBOZU_TEST_EQUAL(f(), 8);
	}
}

CYBOZU_TEST_AUTO(testNewLabel)
{
	struct Code : Xbyak::CodeGenerator {
		Code(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			{
				Label label1;
				Label label2;
				Label label3;
				Label label4;
				Label exit;
				jmp(label1, T_NEAR);
			L(label2);
				inc(eax); // 2
				jmp(label3, T_NEAR);
			L(label4);
				inc(eax); // 4
				jmp(exit, T_NEAR);
				putNop(this, 128);
			L(label3);
				inc(eax); // 3
				jmp(label4, T_NEAR);
			L(label1);
				inc(eax); // 1
				jmp(label2, T_NEAR);
			L(exit);
			}
			{
				Label label1;
				Label label2;
				Label label3;
				Label label4;
				Label exit;
				jmp(label1);
			L(label2);
				inc(eax); // 6
				jmp(label3);
			L(label4);
				inc(eax); // 8
				jmp(exit);
			L(label3);
				inc(eax); // 7
				jmp(label4);
			L(label1);
				inc(eax); // 5
				jmp(label2);
			L(exit);
			}
			Label callLabel;
			{	// eax == 8
				Label label1;
				Label label2;
			L(label1);
				inc(eax); // 9, 10, 11, 13
				cmp(eax, 9);
				je(label1);
				// 10, 11, 13
				inc(eax); // 11, 12, 13
				cmp(eax, 11);
				je(label1);
				// 12, 13
				cmp(eax, 12);
				je(label2);
				inc(eax); // 14
				cmp(eax, 14);
				je(label2);
				ud2();
			L(label2); // 14
				inc(eax); // 13, 15
				cmp(eax, 13);
				je(label1);
			}
			call(callLabel);
			ret();
		L(callLabel);
			inc(eax); // 16
			ret();
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool grow = i == 1;
		printf("testNewLabel grow=%d\n", grow);
		Code code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int r = f();
		CYBOZU_TEST_EQUAL(r, 16);
	}
}

CYBOZU_TEST_AUTO(returnLabel)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			xor_(eax, eax);
		Label L1 = L();
			test(eax, eax);
			Label exit;
			jnz(exit);
			inc(eax); // 1
			Label L2;
			call(L2);
			jmp(L1);
		L(L2);
			inc(eax); // 2
			ret();
		L(exit);
			inc(eax); // 3
			ret();
		}
	};
	Code code;
	int (*f)() = code.getCode<int (*)()>();
	int r = f();
	CYBOZU_TEST_EQUAL(r, 3);
}

CYBOZU_TEST_AUTO(testAssign)
{
	struct Code : Xbyak::CodeGenerator {
		Code(bool grow)
			: Xbyak::CodeGenerator(grow ? 128 : 4096, grow ? Xbyak::AutoGrow : 0)
		{
			xor_(eax, eax);
			Label dst, src;
		L(src);
			inc(eax);
			cmp(eax, 1);
			je(dst);
			inc(eax); // 2, 3, 5
			cmp(eax, 5);
			putNop(this, 128);
			jne(dst, T_NEAR);
			ret();
		assignL(dst, src);
			// test of copy  label
			{
				Label sss(dst);
				{
					Label ttt;
					ttt = src;
				}
			}
		}
	};
	for (int i = 0; i < 2; i++) {
		const bool grow = i == 0;
		printf("testAssign grow=%d\n", grow);
		Code code(grow);
		if (grow) code.ready();
		int (*f)() = code.getCode<int (*)()>();
		int ret = f();
		CYBOZU_TEST_EQUAL(ret, 5);
    }
}

CYBOZU_TEST_AUTO(doubleDefine)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			{
				Label label;
			L(label);
				// forbitten double L()
				CYBOZU_TEST_EXCEPTION(L(label), Xbyak::Error);
			}
			{
				Label label;
				jmp(label);
				CYBOZU_TEST_ASSERT(hasUndefinedLabel());
			}
			{
				Label label1, label2;
			L(label1);
				jmp(label2);
				assignL(label2, label1);
				// forbitten double assignL
				CYBOZU_TEST_EXCEPTION(assignL(label2, label1), Xbyak::Error);
			}
			{
				Label label1, label2;
			L(label1);
				jmp(label2);
				// forbitten assignment to label1 set by L()
				CYBOZU_TEST_EXCEPTION(assignL(label1, label2), Xbyak::Error);
			}
		}
	} code;
}

struct GetAddressCode1 : Xbyak::CodeGenerator {
	void test()
	{
		Xbyak::Label L1, L2, L3;
		nop();
	L(L1);
		const uint8_t *p1 = getCurr();
		CYBOZU_TEST_EQUAL_POINTER(L1.getAddress(), p1);

		nop();
		jmp(L2);
		nop();
		jmp(L3);
	L(L2);
		CYBOZU_TEST_EQUAL_POINTER(L2.getAddress(), getCurr());
		// L3 is not defined
		CYBOZU_TEST_EQUAL_POINTER(L3.getAddress(), 0);

		// L3 is set by L1
		assignL(L3, L1);
		CYBOZU_TEST_EQUAL_POINTER(L3.getAddress(), p1);
	}
};

struct CodeLabelTable : Xbyak::CodeGenerator {
	enum { ret0 = 3 };
	enum { ret1 = 5 };
	enum { ret2 = 8 };
	CodeLabelTable()
	{
		using namespace Xbyak;
#ifdef XBYAK64_WIN
		const Reg64& p0 = rcx;
		const Reg64& a = rax;
#elif defined (XBYAK64_GCC)
		const Reg64& p0 = rdi;
		const Reg64& a = rax;
#else
		const Reg32& p0 = edx;
		const Reg32& a = eax;
		mov(edx, ptr [esp + 4]);
#endif
		Label labelTbl, L0, L1, L2;
		mov(a, labelTbl);
		jmp(ptr [a + p0 * sizeof(void*)]);
	L(labelTbl);
		putL(L0);
		putL(L1);
		putL(L2);
	L(L0);
		mov(a, ret0);
		ret();
	L(L1);
		mov(a, ret1);
		ret();
	L(L2);
		mov(a, ret2);
		ret();
	}
};

CYBOZU_TEST_AUTO(LabelTable)
{
	CodeLabelTable c;
	int (*f)(int) = c.getCode<int (*)(int)>();
	CYBOZU_TEST_EQUAL(f(0), c.ret0);
	CYBOZU_TEST_EQUAL(f(1), c.ret1);
	CYBOZU_TEST_EQUAL(f(2), c.ret2);
}

CYBOZU_TEST_AUTO(getAddress1)
{
	GetAddressCode1 c;
	c.test();
}

struct GetAddressCode2 : Xbyak::CodeGenerator {
	Xbyak::Label L1, L2, L3;
	size_t a1;
	size_t a3;
	explicit GetAddressCode2(int size)
		: Xbyak::CodeGenerator(size, size == 4096 ? 0 : Xbyak::AutoGrow)
		, a1(0)
		, a3(0)
	{
		bool autoGrow = size != 4096;
		nop();
	L(L1);
		if (autoGrow) {
			CYBOZU_TEST_EQUAL_POINTER(L1.getAddress(), 0);
		}
		a1 = getSize();
		nop();
		jmp(L2);
		if (autoGrow) {
			CYBOZU_TEST_EQUAL_POINTER(L2.getAddress(), 0);
		}
	L(L3);
		a3 = getSize();
		if (autoGrow) {
			CYBOZU_TEST_EQUAL_POINTER(L3.getAddress(), 0);
		}
		nop();
		assignL(L2, L1);
		if (autoGrow) {
			CYBOZU_TEST_EQUAL_POINTER(L2.getAddress(), 0);
		}
	}
};

CYBOZU_TEST_AUTO(getAddress2)
{
	const int sizeTbl[] = {
		2, 128, // grow
		4096 // not grow
	};
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(sizeTbl); i++) {
		int size = sizeTbl[i];
		GetAddressCode2 c(size);
		c.ready();
		const uint8_t *p = c.getCode();
		CYBOZU_TEST_EQUAL(c.L1.getAddress(), p + c.a1);
		CYBOZU_TEST_EQUAL(c.L3.getAddress(), p + c.a3);
		CYBOZU_TEST_EQUAL(c.L2.getAddress(), p + c.a1);
	}
}

#ifdef XBYAK64
CYBOZU_TEST_AUTO(rip)
{
	int a[] = { 1, 10 };
	int b[] = { 100, 1000 };
	struct Code : Xbyak::CodeGenerator {
		Code(const int *a, const int *b)
		{
			Label label1, label2;
			jmp("@f");
		L(label1);
			db(a[0], 4);
			db(a[1], 4);
		L("@@");
			mov(eax, ptr [rip + label1]);       // a[0]
			mov(ecx, ptr [rip + label1+4]);     // a[1]
			mov(edx, ptr [rip + label2-8+2+6]); // b[0]
			add(ecx, ptr [rip + 16+label2-12]); // b[1]
			add(eax, ecx);
			add(eax, edx);
			ret();
		L(label2);
			db(b[0], 4);
			db(b[1], 4);

			// error
			CYBOZU_TEST_EXCEPTION(rip + label1 + label2, Xbyak::Error);
		}
	} code(a, b);
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, a[0] + a[1] + b[0] + b[1]);
}

int ret1234()
{
	return 1234;
}

int ret9999()
{
	return 9999;
}

CYBOZU_TEST_AUTO(rip_jmp)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Label label;
			xor_(eax, eax);
			call(ptr [rip + label]);
			mov(ecx, eax);
			call(ptr [rip + label + 8]);
			add(eax, ecx);
			ret();
		L(label);
			db((size_t)ret1234, 8);
			db((size_t)ret9999, 8);
		}
	} code;
	int ret = code.getCode<int (*)()>()();
	CYBOZU_TEST_EQUAL(ret, ret1234() + ret9999());
}

#if 0
CYBOZU_TEST_AUTO(rip_addr)
{
	/*
		we can't assume |&x - &code| < 2GiB anymore
	*/
	static int x = 5;
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			mov(eax, 123);
			mov(ptr[rip + &x], eax);
			ret();
		}
	} code;
	code.getCode<void (*)()>()();
	CYBOZU_TEST_EQUAL(x, 123);
}
#endif

#ifndef __APPLE__
CYBOZU_TEST_AUTO(rip_addr_with_fixed_buf)
{
	MIE_ALIGN(4096) static char buf[8192];
	static char *p = buf + 4096;
	static int *x0 = (int*)buf;
	static int *x1 = x0 + 1;
	struct Code : Xbyak::CodeGenerator {
		Code() : Xbyak::CodeGenerator(4096, p)
		{
			mov(eax, 123);
			mov(ptr[rip + x0], eax);
			mov(dword[rip + x1], 456);
			mov(byte[rip + 1 + x1 + 3], 99);
			ret();
		}
	} code;
	code.setProtectModeRE();
	code.getCode<void (*)()>()();
	CYBOZU_TEST_EQUAL(*x0, 123);
	CYBOZU_TEST_EQUAL(*x1, 456);
	CYBOZU_TEST_EQUAL(buf[8], 99);
	code.setProtectModeRW();
}
#endif

CYBOZU_TEST_AUTO(ripLabel)
{
	const uint8_t ok[] = {
		0xF3, 0x0F, 0xC2, 0x05, 0xF1, 0x00, 0x00, 0x00, 0x00,
		0xF7, 0x05, 0xE7, 0x00, 0x00, 0x00, 0x21, 0x00, 0x00, 0x00,
		0x0F, 0xBA, 0x25, 0xDF, 0x00, 0x00, 0x00, 0x03,
		0xC4, 0xE3, 0x79, 0x0D, 0x05, 0xD5, 0x00, 0x00, 0x00, 0x03,
		0xC4, 0xE3, 0x79, 0x0F, 0x05, 0xCB, 0x00, 0x00, 0x00, 0x04,
		0xC4, 0xE3, 0x7D, 0x19, 0x1D, 0xC1, 0x00, 0x00, 0x00, 0x0C,
		0xC4, 0xE3, 0x75, 0x46, 0x05, 0xB7, 0x00, 0x00, 0x00, 0x0D,
		0xC4, 0xE3, 0x79, 0x1D, 0x15, 0xAD, 0x00, 0x00, 0x00, 0x2C,
		0xC7, 0x05, 0xA3, 0x00, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00,
		0xC1, 0x25, 0x9C, 0x00, 0x00, 0x00, 0x03,
		0xD1, 0x2D, 0x96, 0x00, 0x00, 0x00,
		0x48, 0x0F, 0xA4, 0x05, 0x8D, 0x00, 0x00, 0x00, 0x03,
		0x48, 0x6B, 0x05, 0x85, 0x00, 0x00, 0x00, 0x15,
		0xC4, 0xE3, 0xFB, 0xF0, 0x05, 0x7B, 0x00, 0x00, 0x00, 0x15,
		0xF7, 0x05, 0x71, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
		0x66, 0x48, 0x0F, 0x3A, 0x16, 0x05, 0x66, 0x00, 0x00, 0x00, 0x03,
		0x66, 0x48, 0x0F, 0x3A, 0x22, 0x15, 0x5B, 0x00, 0x00, 0x00, 0x05,
		0x66, 0x0F, 0x3A, 0x15, 0x0D, 0x51, 0x00, 0x00, 0x00, 0x04,
		0x81, 0x15, 0x47, 0x00, 0x00, 0x00, 0x45, 0x23, 0x01, 0x00,
		0x0F, 0xBA, 0x25, 0x3F, 0x00, 0x00, 0x00, 0x34,
		0x66, 0x0F, 0xBA, 0x3D, 0x36, 0x00, 0x00, 0x00, 0x34,
		0x0F, 0xBA, 0x35, 0x2E, 0x00, 0x00, 0x00, 0x34,
		0xC1, 0x15, 0x27, 0x00, 0x00, 0x00, 0x04,
		0x48, 0x0F, 0xA4, 0x05, 0x1E, 0x00, 0x00, 0x00, 0x04,
		0x0F, 0x3A, 0x0F, 0x05, 0x15, 0x00, 0x00, 0x00, 0x04,
		0x66, 0x0F, 0x3A, 0xDF, 0x1D, 0x0B, 0x00, 0x00, 0x00, 0x04,
		0xC4, 0xE3, 0x79, 0x60, 0x15, 0x01, 0x00, 0x00, 0x00, 0x07,
		0xC3,
		0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
	};
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Xbyak::Label label;
			cmpss(xmm0, ptr[rip + label], 0);
			test(dword[rip + label], 33);
			bt(dword[rip + label ], 3);
			vblendpd(xmm0, xmm0, dword[rip + label], 3);
			vpalignr(xmm0, xmm0, qword[rip + label], 4);
			vextractf128(dword[rip + label], ymm3, 12);
			vperm2i128(ymm0, ymm1, qword[rip + label], 13);
			vcvtps2ph(ptr[rip + label], xmm2, 44);
			mov(dword[rip + label], 0x1234);
			shl(dword[rip + label], 3);
			shr(dword[rip + label], 1);
			shld(qword[rip + label], rax, 3);
			imul(rax, qword[rip + label], 21);
			rorx(rax, qword[rip + label], 21);
			test(dword[rip + label], 5);
			pextrq(ptr[rip + label], xmm0, 3);
			pinsrq(xmm2, ptr[rip + label], 5);
			pextrw(ptr[rip + label], xmm1, 4);
			adc(dword[rip + label], 0x12345);
			bt(byte[rip + label], 0x34);
			btc(word[rip + label], 0x34);
			btr(dword[rip + label], 0x34);
			rcl(dword[rip + label], 4);
			shld(qword[rip + label], rax, 4);
			palignr(mm0, ptr[rip + label], 4);
			aeskeygenassist(xmm3, ptr[rip + label], 4);
			vpcmpestrm(xmm2, ptr[rip + label], 7);
			ret();
		L(label);
			dq(0x123456789abcdef0ull);
		};
	} c;
	CYBOZU_TEST_EQUAL(sizeof(ok), c.getSize());
	CYBOZU_TEST_EQUAL_ARRAY(ok, c.getCode(), sizeof(ok));
}
#endif

struct ReleaseTestCode : Xbyak::CodeGenerator {
	ReleaseTestCode(Label& L1, Label& L2, Label& L3)
	{
		L(L1);
		jmp(L1);
		L(L2);
		jmp(L3); // not assigned
	}
};

/*
	code must unlink label if code is destroyed
*/
CYBOZU_TEST_AUTO(release_label_after_code)
{
	puts("---");
	{
		Label L1, L2, L3, L4, L5;
		{
			ReleaseTestCode code(L1, L2, L3);
			CYBOZU_TEST_ASSERT(L1.getId() > 0);
			CYBOZU_TEST_ASSERT(L1.getAddress() != 0);
			CYBOZU_TEST_ASSERT(L2.getId() > 0);
			CYBOZU_TEST_ASSERT(L2.getAddress() != 0);
			CYBOZU_TEST_ASSERT(L3.getId() > 0);
			CYBOZU_TEST_ASSERT(L3.getAddress() == 0); // L3 is not assigned
			code.assignL(L4, L1);
			L5 = L1;
			printf("id=%d %d %d %d %d\n", L1.getId(), L2.getId(), L3.getId(), L4.getId(), L5.getId());
		}
		puts("code is released");
		CYBOZU_TEST_ASSERT(L1.getId() == 0);
		CYBOZU_TEST_ASSERT(L1.getAddress() == 0);
		CYBOZU_TEST_ASSERT(L2.getId() == 0);
		CYBOZU_TEST_ASSERT(L2.getAddress() == 0);
//		CYBOZU_TEST_ASSERT(L3.getId() == 0); // L3 is not assigned so not cleared
		CYBOZU_TEST_ASSERT(L3.getAddress() == 0);
		CYBOZU_TEST_ASSERT(L4.getId() == 0);
		CYBOZU_TEST_ASSERT(L4.getAddress() == 0);
		CYBOZU_TEST_ASSERT(L5.getId() == 0);
		CYBOZU_TEST_ASSERT(L5.getAddress() == 0);
		printf("id=%d %d %d %d %d\n", L1.getId(), L2.getId(), L3.getId(), L4.getId(), L5.getId());
	}
}

struct JmpTypeCode : Xbyak::CodeGenerator {
	void nops()
	{
		for (int i = 0; i < 130; i++) {
			nop();
		}
	}
	// return jmp code size
	size_t gen(bool pre, bool large, Xbyak::CodeGenerator::LabelType type)
	{
		Label label;
		if (pre) {
			L(label);
			if (large) nops();
			size_t pos = getSize();
			jmp(label, type);
			return getSize() - pos;
		} else {
			size_t pos = getSize();
			jmp(label, type);
			size_t size = getSize() - pos;
			if (large) nops();
			L(label);
			return size;
		}
	}
};

CYBOZU_TEST_AUTO(setDefaultJmpNEAR)
{
	const Xbyak::CodeGenerator::LabelType T_SHORT = Xbyak::CodeGenerator::T_SHORT;
	const Xbyak::CodeGenerator::LabelType T_NEAR = Xbyak::CodeGenerator::T_NEAR;
	const Xbyak::CodeGenerator::LabelType T_AUTO = Xbyak::CodeGenerator::T_AUTO;
	const struct {
		bool pre;
		bool large;
		Xbyak::CodeGenerator::LabelType type;
		size_t expect1; // 0 means exception
		size_t expect2;
	} tbl[] = {
		{ false, false, T_SHORT, 2, 2 },
		{ false, false, T_NEAR, 5, 5 },
		{ false, true, T_SHORT, 0, 0 },
		{ false, true, T_NEAR, 5, 5 },

		{ true, false, T_SHORT, 2, 2 },
		{ true, false, T_NEAR, 5, 5 },
		{ true, true, T_SHORT, 0, 0 },
		{ true, true, T_NEAR, 5, 5 },

		{ false, false, T_AUTO, 2, 5 },
		{ false, true, T_AUTO, 0, 5 },
		{ true, false, T_AUTO, 2, 2 },
		{ true, true, T_AUTO, 5, 5 },
	};
	JmpTypeCode code1, code2;
	code2.setDefaultJmpNEAR(true);
	for (size_t i = 0; i < CYBOZU_NUM_OF_ARRAY(tbl); i++) {
		if (tbl[i].expect1) {
			size_t size = code1.gen(tbl[i].pre, tbl[i].large, tbl[i].type);
			CYBOZU_TEST_EQUAL(size, tbl[i].expect1);
		} else {
			CYBOZU_TEST_EXCEPTION(code1.gen(tbl[i].pre, tbl[i].large, tbl[i].type), std::exception);
		}
		if (tbl[i].expect2) {
			size_t size = code2.gen(tbl[i].pre, tbl[i].large, tbl[i].type);
			CYBOZU_TEST_EQUAL(size, tbl[i].expect2);
		} else {
			CYBOZU_TEST_EXCEPTION(code2.gen(tbl[i].pre, tbl[i].large, tbl[i].type), std::exception);
		}
	}
}

CYBOZU_TEST_AUTO(isDefined)
{
	struct Code : Xbyak::CodeGenerator {
		Code()
		{
			Label L1, L2;
			CYBOZU_TEST_ASSERT(!L1.isDefined());
			CYBOZU_TEST_ASSERT(!L2.isDefined());
			L(L1);
			jmp(L2);
			CYBOZU_TEST_ASSERT(L1.isDefined());
			CYBOZU_TEST_ASSERT(!L2.isDefined());
			L(L2);
			CYBOZU_TEST_ASSERT(L1.isDefined());
			CYBOZU_TEST_ASSERT(L2.isDefined());
		}
	} code;
}

CYBOZU_TEST_AUTO(ambiguousFarJmp)
{
	struct Code : Xbyak::CodeGenerator {
#ifdef XBYAK32
		void genJmp() { jmp(ptr[eax], T_FAR); }
		void genCall() { call(ptr[eax], T_FAR); }
#else
		void genJmp() { jmp(ptr[rax], T_FAR); }
		void genCall() { call(ptr[rax], T_FAR); }
#endif
	} code;
	CYBOZU_TEST_EXCEPTION(code.genJmp(), std::exception);
	CYBOZU_TEST_EXCEPTION(code.genCall(), std::exception);
}
