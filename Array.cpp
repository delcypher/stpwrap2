#include "Array.h"
#include <iostream>
using namespace std;

Array::Array() : indexBitWidth(32), valueBitWidth(8) , rawBytes() { }

void Array::addIndex(int index)
{
	if( index >= rawBytes.size())
	{
		//Vector currently isn't big enough. Resize and fill elements with 0
		rawBytes.resize(index +1, 0);
	}

}


bool Array::setByte(int index, unsigned char value)
{
	if(index >= rawBytes.size())
		return false;

	rawBytes[index]=value;
	return true;
}

unsigned char Array::getByte(int index)
{
	if( index > rawBytes.size())
		cerr << "Array::getByte() : Invalid index!" << endl;

	return rawBytes[index];
}
