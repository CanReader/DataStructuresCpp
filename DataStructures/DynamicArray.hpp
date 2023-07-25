#ifndef _DYNAMICARRAY_H_
#define _DYNAMICARRAY_H_

#define ArraySize(arr) sizeof(arr)/sizeof(arr[0])

#include "DSBase.hpp"
#include <ostream>

namespace ADS
{
	template<typename T>
	class DynamicArray : public __Base::IDSBase<T>
	{
	public:
		DynamicArray()
		{
			this->arr = new T[this->Capacity];
		}

		DynamicArray(T InitItem)
		{
			this->arr = new T[this->Capacity];
			Insert(0, InitItem);
		}

		DynamicArray(T* InitItem)
		{
			this->arr = InitItem;
		}

		~DynamicArray()
		{
			delete[] this->arr;
		}

		T* ToArray()
		{
			return this->arr;
		}

		template<typename T>
		friend std::ostream& operator<<(std::ostream& out, const DynamicArray<T>& c)
		{

			for (int i = 0; i < c.Size; i++)
				out << c.arr[i] << ", ";
			return out;
		}

		T& operator +=(const T& Value)
		{
			Add(Value);
			return *this;
		}

		void Change(size_t Index, const T Value);
		void Insert(size_t Index, const T Value);
		T& At(size_t Index);
		void Add(const T Value);
		void Add(const T* Value, size_t Size);
		void EraseAt(int Index);
		void EraseAt(int First, int Last);
		void Erase(const T& Value);
		void EraseAll(const T& Value);

		int  IndexOf(const T Value) noexcept;
		bool Contains(const T Value) noexcept;
		void Reverse();
		T* Copy();

		void Clear();

	private:
	protected:
	private:
	};

	template<typename T>
	inline void DynamicArray<T>::Change(size_t Index, const T Value)
	{
		if (Index < this->Size && Index > 0)
			this->arr[Index] = Value;
		else
			throw std::out_of_range("Index out of bounds.");
	}

	template<typename T>
	inline void DynamicArray<T>::Insert(size_t Index, const T Value)
	{
		if (Index > 0 && Index < this->Size)
		{
		this->IncreaseCapacity();
		T* temp = new T[this->Capacity];

		for (int i = 0; i < Index; i++)
			temp[i] = this->arr[i];

		temp[Index] = Value;

		for (int j = Index; j < this->Size; j++)
			temp[j + 1] = this->arr[j];

		delete[] this->arr;
		this->arr = temp;
		this->Size++;
		}
		else
		{
			throw std::out_of_range("Index out of bounds.");
		}
	}

	template<typename T>
	inline T& DynamicArray<T>::At(size_t Index)
	{
		if (Index < this->Size && Index > 0)
			return this->arr[Index];
		else
			throw std::out_of_range("Index out of bounds.");
	}

	template<typename T>
	inline void DynamicArray<T>::Add(const T Value)
	{
		if (this->Capacity <= this->Size)
		{
			this->IncreaseArray(2);
		}
			this->arr[this->Size] = Value;
			this->Size++;
	}
	template<typename T>
	inline void DynamicArray<T>::Add(const T* Value, size_t Size)
	{
		if (this->Capacity <= this->Size)
		{
			this->IncreaseArray(2);
		}

		IncreaseArrayBy(this->Size + Size);

		for (int i = 0; i < Size - 1; i++)
		{
			this->arr[this->Size + i] = Value[i];
		}
	}

	template<typename T>
	inline void DynamicArray<T>::EraseAt(int Index)
	{
		if (Index < this->Size && Index > 0)
		{
			for (int i = Index; i < this->Size - 1; i++)
				this->arr = this->arr[i + 1];
			this->Size--;
		}
		else
			throw std::out_of_range("Index out of bounds.");
	}

	template<typename T>
	inline void DynamicArray<T>::EraseAt(int First, int Last)
	{
		if (First >= 0 && Last >= First && Last < this->Size)
		{
			const int numElementsToRemove = Last - First + 1;
			for (int i = First; i < this->Size - numElementsToRemove; i++)
				this->arr[i] = this->arr[i + numElementsToRemove];

			this->Size -= numElementsToRemove;
		}
		else
		{
			throw std::out_of_range("Index out of bounds.");
		}
	}

	template<typename T>
	inline void DynamicArray<T>::Erase(const T& Value)
	{
		for (int i = 0; i < this->Size; i++)
		{
			if (this->arr[i] == Value)
			{
				for (int j = i; j < this->Size; j++)
				{
					this->arr[j] = this[j + 1];
				}
				this->Size--;
				break;
			}
		}
	}

	template<typename T>
	inline void DynamicArray<T>::EraseAll(const T& Value)
	{
		for (int i = 0; i < this->Size; i++)
		{
			if (this->arr[i] == Value)
			{
				for (int j = i; j < this->Size; j++)
				{
					this->arr[j] = this[j + 1];
				}
				this->Size--;
			}
		}
	}

	template<typename T>
	inline int DynamicArray<T>::IndexOf(const T Value) noexcept
	{
		for (int i = 0; i < this->Size-1; i++)
		{
			if (this->arr[i] == Value)
				return i;
		}
		return -1;
	}

	template<typename T>
	inline bool DynamicArray<T>::Contains(const T Value) noexcept
	{
		if (IndexOf(Value) == -1)
			return false;
		else
			return true;
	}

	template<typename T>
	inline void DynamicArray<T>::Reverse()
	{
		int Left = 0;
		int Right = this->Size - 1;

		while (Right > Left)
		{
			T temp = this->arr[Right];
			this->arr[Left] = this->arr[Right];
			this->arr[Right] = temp;

			Right--;
			Left++;
		}
	}

	template<typename T>
	inline T* DynamicArray<T>::Copy()
	{
		T* temp = new T[this->Capacity];

		for (int i = 0; i < this->Size; i++)
		{
			temp[i] = this->arr[i];
		}

		return temp;
	}

	template<typename T>
	inline void DynamicArray<T>::Clear()
	{
		EraseAt(0,this->Size-1);
		delete[] this->arr;
		this->arr = new T[this->Capacity];
	}
}

#endif