#ifndef ARRAY_H
#define ARRAY_H 1
#include <vector>

class Array
{
	public:
		Array();
		int getNumberOfElements();

		//Set byte with offset "index" to value "value"
		//Returns true is successful.
		//May fail because index is invalid
		bool setByte(int index,char value);
		
		//Add an index with value set to zero
		//If index already exists nothing is changed.
		//addIndex() does not need to be called with monotonically increasing values of "index".
		void addIndex(int index);

		char getByte(int index);

		int indexBitWidth;
		int valueBitWidth;
	private:
		std::vector<char> rawBytes;


};

#endif
