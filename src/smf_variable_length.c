#include <stdio.h>
#include <stdint.h>

uint32_t smf_vl_bits(uint32_t);

// 32bit version
int smf_vl_encode(uint32_t v, uint8_t* s, uint32_t sz)
{
	uint32_t n_bits = smf_vl_bits(v);
	uint32_t n_7bits = (n_bits+6) / 7;
	if (!n_7bits) n_7bits = 1;

	// not enough bytes
	if (sz < n_7bits) return 0;

	uint8_t* lsb = s + (n_7bits-1);
	uint32_t cur_v = v;

	for (uint8_t* cur=lsb; s <= cur; --cur, cur_v>>=7) {
		*cur = cur_v | 0x80;
	}
	*lsb &= 0x7F;

	return n_7bits;
}

int smf_vl_decode(const uint8_t* s, uint32_t sz, uint32_t* val)
{
	const int8_t* end = s + sz;
	const int8_t* c = s;

	*val = 0;
	// TODO: check if input stream size fits to uint32_t
	for (; c<end; ++c) {
		*val <<= 7;
		*val |= (*c & 0x7F);

		// if the MSB isn't set
		if (*c >= 0) {
			++c;
			break;
		}
	}

	return (*c<0)? 0: c-(const int8_t*)s;
}

// https://www.geeksforgeeks.org/find-significant-set-bit-number/
// https://www.codegrepper.com/code-examples/cpp/how+to+find+position+of+lsb+and+msb+in+c
// https://codeforces.com/blog/entry/10330
// https://hg.openjdk.java.net/jdk8/jdk8/jdk/file/687fd7c7986d/src/share/classes/java/lang/Integer.java
// Integer.numberOfLeadingZeros()
uint32_t smf_vl_bits(uint32_t v)
{
	if (!v) return 0;

	uint32_t n = 1;
	if (!(v >> 16)) { n+=16; v<<=16; }
	if (!(v >> 24)) { n+=8; v<<=8; }
	if (!(v >> 28)) { n+=4; v<<=4; }
	if (!(v >> 30)) { n+=2; v<<=2; }
	n -= v >> 31;

	return 32-n;
}

void bin_str(uint32_t v)
{
	static const char* n2bs[] = {
		"0000", "0001", "0010", "0011",
		"0100", "0101", "0110", "0111",
		"1000", "1001", "1010", "1011",
		"1100", "1101", "1110", "1111",
	};

	for (int i=24; i>=0; i-=8) {
		uint8_t b = (v>>i)&0xFF;
		//puts(n2bs[b>>4]);
		//puts(n2bs[b&0x0F]);
		//putchar(' ');
		printf("%s%s ", n2bs[b>>4], n2bs[b&0x0F]);
	}
}

int XXmain()
{
	//uint32_t i = 1;
	//for (int s=0; i<(uint32_t)-1; s+=8) {
	for (int s=0; 1; s+=8) {
		uint32_t i = 1 << s;
		printf("%X:\t%u,\t", i, smf_vl_bits(i));
		bin_str(i);
		putchar('\n');

		if (!i) break;
	}

	return 0;
}

int main()
{
	for (uint32_t i=0; i<0xFFFFFFFF; ++i) {
		uint8_t buf[8] = {0};
		int sz = smf_vl_encode(i, buf, 8);

		uint32_t result;
		int sz2 = smf_vl_decode(buf, 8, &result);

		if (i != result || sz != sz2) {
			printf("%u fail\n", i);
			printf("sz: %d, result: %u, sz2: %d\n", sz, result, sz2);
			break;
		}

		if (!(i & 0xFF)) {
			printf("%u OK\n", i);
		}
	}

	return 0;
}
