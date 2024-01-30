#ifndef _OPENSSL_RC5_H_
#define _OPENSSL_RC5_H_

#ifdef  __cplusplus
extern "C" {
#endif

#define RC5_ENCRYPT	1
#define RC5_DECRYPT	0


#define RC5_32_INT unsigned int

#define RC5_32_BLOCK		8
#define RC5_32_KEY_LENGTH	16 


#define RC5_8_ROUNDS	8
#define RC5_12_ROUNDS	12
#define RC5_16_ROUNDS	16

#ifndef ROTATE_l32
#define ROTATE_l32(a,n)     (((a)<<(n&0x1f))|(((a)&0xffffffff)>>(32-(n&0x1f))))
#endif
#ifndef ROTATE_r32
#define ROTATE_r32(a,n)     (((a)<<(32-(n&0x1f)))|(((a)&0xffffffff)>>(n&0x1f)))
#endif

#define RC5_32_MASK	0xffffffffL

#define RC5_16_P	0xB7E1
#define RC5_16_Q	0x9E37
#define RC5_32_P	0xB7E15163L
#define RC5_32_Q	0x9E3779B9L
#define RC5_64_P	0xB7E151628AED2A6BLL
#define RC5_64_Q	0x9E3779B97F4A7C15LL

#define E_RC5_32(a,b,s,n) \
	a^=b; \
	a=ROTATE_l32(a,b); \
	a+=s[n]; \
	a&=RC5_32_MASK; \
	b^=a; \
	b=ROTATE_l32(b,a); \
	b+=s[n+1]; \
	b&=RC5_32_MASK;

#define D_RC5_32(a,b,s,n) \
	b-=s[n+1]; \
	b&=RC5_32_MASK; \
	b=ROTATE_r32(b,a); \
	b^=a; \
	a-=s[n]; \
	a&=RC5_32_MASK; \
	a=ROTATE_r32(a,b); \
	a^=b;


typedef struct rc5_key_st
	{
	
	int rounds;
	RC5_32_INT data[2*(RC5_16_ROUNDS+1)];
	} RC5_32_KEY;

 
void RC5_32_set_key(RC5_32_KEY *key, int len, const unsigned char *data,
	int rounds);
void RC5_32_encrypt( RC5_32_INT *data,RC5_32_KEY *key);
void RC5_32_decrypt(RC5_32_INT *data,RC5_32_KEY *key);

#ifdef  __cplusplus
}
#endif

#endif


