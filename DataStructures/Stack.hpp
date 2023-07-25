#ifndef _STACK_H_
#define _STACK_H_

#include "DSBase.h"


namespace ADS
{
	template <typename T>
	class Stack : public IDSBase<T>
	{
	public:
		Stack()
		{
			this->arr = (T*)malloc(sizeof(T) * 2);
		}

		Stack(T InitItem)
		{
			this->arr = (T*)malloc(sizeof(T) * 2);
			Push(InitItem);
		}
		Stack(T InitItem[])
		{
			this->arr = InitItem;
			this->Size = sizeof(InitItem) / sizeof(InitItem[0]);
			this->Capacity = this->Size * 2;
		}

		void Push(T);
		T Pop();

		friend std::ostream& operator << (std::ostream& out, const Stack<T>& c)
		{
			out << std::endl;

			out << "**---------------------------------------------------------**" << std::endl;
			for (int i = 0; i < c.Size; i++)
				out << "Element " << i + 1 << ". " << c.arr[i] << std::endl;
			out << "**---------------------------------------------------------**" << std::endl;

			return out;
		}

		void Print();
		void Clear();
	};
}

//Definitions

template<typename T>
inline void ADS::Stack<T>::Push(T Item)
{
	ExecuteIfBigger(this->Size, this->Capacity)
		this->Expand(2);

	this->arr[this->Size++] = Item;
}

template<typename T>
inline T ADS::Stack<T>::Pop()
{
	if (this->Size == 0)
	{
		std::cout << "Unable to pop any more, because the stack is empty..." << std::endl;
		return T();
	}
	return this->arr[--this->Size];
}

template<typename T>
inline void ADS::Stack<T>::Print()
{
	std::cout << std::endl;

	std::cout << "**---------------------------------------------------------**" << std::endl;
	for (int i = 0; i < this->Size; i++)
		std::cout << "Element " << i + 1 << ". " << this->arr[i] << std::endl;
	std::cout << "**---------------------------------------------------------**" << std::endl;

}

template<typename T>
inline void ADS::Stack<T>::Clear()
{
	for (int i = 0; i < this->Size; i++)
		this->arr[i] = NULL;
}

#endif