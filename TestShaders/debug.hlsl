// Test case for the debug layer instructions
//   Compile with fxc /T cs_5_0 /Od /Fo debug.bin

Buffer<uint> ubuf : register(t0);
Buffer<float> fbuf : register(t1);

  [numthreads(1, 1, 1)]
void main()
{
	uint tally = 0; // Used to thwart compiler optimisations
	uint ret;

	printf("simple printf test");
	printf("printf embedded \"quote\" test");
	printf("printf naive parser test\", r0.x");
	printf("newline escape\n test");
	printf("literal newline \
	test");
	printf("Escape sequence test bksp \b esc \e nl \n cr \r tab \t bksl \\ sq \' dq \" qm \? db bs \\\\ escaped nl \\n eol \\");
	//printf("Ambiguous escape sequence bel \a"); // Disassembler encodes to an ambiguous "."
	//printf("Ambiguous escape sequence f \f"); // Disassembler encodes to an ambiguous "."
	//printf("Ambiguous escape sequence vtab \v"); // Disassembler encodes to an ambiguous "."
	printf("C++ comment // character and \"quote\" \// to fool the parser");
	printf("printf integer test: %d", ubuf[0]);
	printf("printf literal float test: %f", 3.14159);
	printf("printf 1 value test: %f", fbuf[0]);
	printf("printf 2 values test: %d %d", ubuf[0], ubuf[1]);
	printf("printf 3 values test: %d %d %d", ubuf[0], ubuf[1], ubuf[2]);
	printf("printf 4 values test: %d %d %d", ubuf[0], ubuf[1], ubuf[2], ubuf[3]);
	printf("printf 5 values test: %d %i %x %d", ubuf[0], ubuf[1], ubuf[2], ubuf[3], ubuf[4]);
	printf("printf 6 values test: %d %d %i %x %d", ubuf[0], ubuf[1], ubuf[2], ubuf[3], ubuf[4], ubuf[5]);
	// 7 parameters including the format string is the limit in older versions of fxc (SDK 8.0)
	//printf("printf 7 values test: %d %d %i %x %d", ubuf[0], ubuf[1], ubuf[2], ubuf[3], ubuf[4], ubuf[5], ubuf[6]);
	//printf("printf 8 values test: %d %d %i %x %d", ubuf[0], ubuf[1], ubuf[2], ubuf[3], ubuf[4], ubuf[5], ubuf[6], ubuf[7]);
	// 9 parameters including the format string is the limit in newer versions of fxc

	// abort
	if (ubuf[1] > 4)
		abort();

	if (ubuf[2] > 4)
		errorf("simple errorf test");

	if (ubuf[3] > 4)
		errorf("errorf integer test: %d", ubuf[3]);
}
