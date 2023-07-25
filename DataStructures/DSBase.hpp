#ifndef _BSBASE_H_
#define _BSBASE_H_

#include <optional>

#define ExecuteIfBigger(Capacity,Size) if(Capacity <= Size)
#define RunForSize(Size) for(int i = 0; i < Size; i++)

namespace ADS
{
	namespace __Base
	{

		template<typename T>
		class IDSBase
		{
		public:
			virtual void Clear() = 0;
			virtual bool IsEmpty() { return this->Size == 0; }
			virtual T* ToArray() = 0;
			virtual int GetSize() { return this->Size; }

		protected:
			bool CheckSize()
			{
				return Size >= Capacity;
			}

			void IncreaseCapacity()
			{
				Capacity *= 2;
			}

			void IncreaseCapacity(int Value)
			{
				Capacity *= Value;
			}

			void IncreaseArray()
			{
				T* temp = new T[Capacity];

				for (int i = 0; i < Size; i++)
				{
					temp[i] = arr[i];
				}

				free(arr);
				arr = temp;
			}

			void IncreaseArray(int Value)
			{
				IncreaseCapacity(Value);

				T* temp = new T[Capacity];

				for (int i = 0; i < Size; i++)
				{
					temp[i] = arr[i];
				}

				delete arr;
				arr = temp;
			}

			void IncreaseArrayBy(int Value)
			{
				T* temp = new T[Value];

				for (int i = 0; i < Size; i++)
				{
					temp[i] = arr[i];
				}

				delete arr;
				arr = temp;
			}

			int Size = 0;
			int Capacity = 2;
			T* arr;

		public:
			T* First;
			T* Last;

		};
	}
}
#endif // !