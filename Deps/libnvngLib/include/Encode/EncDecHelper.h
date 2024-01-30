#pragma  once

#include "OpenSSLDec.h"
#include "OpenSSLRC5.h"

class EncDecHelper
{
	public :
		enum EncDecMethod
		{
			Method_Des =1,
			Method_RC5 =2,
			Method_None =3,
		};
	public :
		

		unsigned int encDec(void * data,unsigned int len,bool enc);

		

		void desEncDec(void * data,unsigned int len,bool enc);

		
		
		void rc5EncDec(void * data,unsigned int len,bool enc);

		void setEncDecMethod(EncDecMethod method)
		{
			this->encDecmethod = method;
		}	
		EncDecMethod getEncDecMethod() const
		{
			return this->encDecmethod;
		}
		void setEncMask(unsigned int mask)
		{
			this->encMask = mask;	
		}
		void setDecMask(unsigned int mask)
		{
			this->decMask = mask;
		}
		void des_set_key(const_DES_cblock *key);
		
		void rc5_set_key(unsigned int len, const unsigned char *data,int rounds);
	private :
		EncDecMethod encDecmethod;
		DES_key_schedule desKey;
		RC5_32_KEY  rc5Key;
		bool  isSetDesKey;	
		bool  isSetRC5Key;	
		unsigned int encMask;		
		unsigned int decMask;		
};



