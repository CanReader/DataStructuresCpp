#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "DSBase.hpp"

namespace ADS
{

	template<typename T>
	class Queue : public IDSBase<T>
	{
	public:
		Queue()
		{
			this->arr = (T*)malloc(sizeof(T) * 2);
			this->Capacity = 2;
			this->Size = 0;
		}

		Queue(T InitItem)
		{
			this->arr = (T*)malloc(sizeof(T) * 2);
			Enqueue(InitItem);
		}

		Queue(T InitItem[])
		{
			this->arr = InitItem;
			this->Size = sizeof(InitItem) / sizeof(InitItem[0]);
			this->Capacity = this->Size * 2;
		}

		void Print();
		void Clear();

		void Enqueue(T);
		std::optional<T> Dequeue();

		T First();
		T End();

	private:

	};

	template<typename T>
	inline void Queue<T>::Print()
	{
		std::cout << std::endl;

		std::cout << "**---------------------------------------------------------**" << std::endl;
		for (int i = 0; i < this->Size; i++)
			std::cout << this->arr[i] << " ";
		std::cout << std::endl << "**---------------------------------------------------------**" << std::endl;
	}

	template<typename T>
	inline void Queue<T>::Clear()
	{
	}

	template<typename T>
	inline void Queue<T>::Enqueue(T Item)
	{
		if (this->CheckSize())
			this->Expand(2);

		this->arr[this->Size++] = Item;
	}

	/*
	template<typename T>
	inline std::optional<T> Queue<T>::Dequeue()
	{
		if (this->Size == 0)
			return NULL;

		T result = this->arr[0];

		for (int i = 0; i < this->Size; i++)
			if (this->arr[i + 1] != NULL)
				this->arr[i] = this->arr[i + 1];

		this->Size--;

		return result;
	}
	*/

	template<typename T>
	inline T Queue<T>::First()
	{
		return this->arr[0];
	}

	template<typename T>
	inline T Queue<T>::End()
	{
		return this->arr[this->Size];
	}

}

#endif // !1
