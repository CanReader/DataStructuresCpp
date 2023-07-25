#ifndef _LIST_H_
#define _LIST_H_

#include <optional>
#include <stdlib.h>

//template <typename T>


template <typename T>
class List
{
public:
	List();
	List(T InitItem)
	{
		root = InitNode(InitItem, 0, nullptr, nullptr);

		tail = root;

		Size = 1;
	}
	List(T InitItem[], int ArraySize)
	{
		root = InitNode(InitItem[0], 0, nullptr, nullptr);

		Size = 1;

		iter = root;

		for (int i = 1; i < ArraySize; i++)
		{
			iter->Next = InitNode(InitItem[i], i, iter, nullptr);

			iter = iter->Next;
		}

		iter->Next = nullptr;

		tail = iter;

		Size = ArraySize + 1;
	}

	void Append(T Data);
	void Insert(int index, T Data);
	void Pop();
	void Remove(T Data);
	void Extend(T Datas[], int ArraySize);
	void Extend(int index, T Datas[], int ArraySize);
	void Clear();

	int GetSize() { return Size; }
	int IndexOf(T Data);
	std::optional<T> At(int index);
	std::optional<T> Find(T Data);
	T* ToArray();

	void Print();

private:
	struct Node
	{
		T Data;
		int Index;
		Node* Prev;
		Node* Next;
	};

	struct Node* root;
	struct Node* iter;
	struct Node* tail;

	int Size;

	Node* InitNode(T Data, int Index, struct Node* Prev, struct Node* Next)
	{
		Node* n = (Node*)malloc(sizeof(Node));

		n->Data = Data;
		n->Index = Index;
		n->Next = Next;
		n->Prev = Prev;

		return n;
	}

	void ResortIndices(Node* Start)
	{
		iter = Start;
		int Starti = Start->Index;

		int i = 0;
		while (iter != tail)
		{
			iter->Index = Starti + i;
			iter = iter->Next;
			i++;
		}
	}

	std::optional<Node> FindNode(T Data)
	{
		iter = root;

		while (iter != tail)
		{
			if (iter->Data == Data)
				return iter;
			iter = iter->Next;
		}

		return nullptr;
	}

	Node FindNodeAt(int index);
};


template<typename T>
inline void List<T>::Append(T Data)
{
	iter = tail;
	if (Size == 0)
	{
		root = InitNode(Data, 0, nullptr, nullptr);
		tail = root;
	}
	else
	{
		tail->Next = InitNode(Data, Size - 1, tail, nullptr);
		tail = tail->Next;
	}
	Size++;
}

template<typename T>
inline void List<T>::Insert(int index, T Data)
{
	iter = root;

	if (index > Size)
		return;

	while (iter->Index != index)
		iter = iter->Next;

	iter->Prev->Next = InitNode(Data, index, iter->Prev, iter);
	iter->Prev->Next->Next->Prev = iter->Prev->Next;
	iter = iter->Prev->Next;

	ResortIndices(iter);

	Size++;
}

template<typename T>
inline void List<T>::Pop()
{
	if (tail == root)
	{
		if (Size != 0)free(root);
		Size = 0;
		return;
	}

	tail = tail->Prev;
	free(tail->Next);
	Size--;
}

template<typename T>
inline void List<T>::Remove(T Data)
{
	if (Size = 0 || tail == root)
		return;

	iter = root;

	while (iter->Data != Data)
		if (iter != tail)
			iter = iter->Next;
		else
			return;

	iter->Prev->Next = iter->Next;
	iter->Next->Prev = iter->Prev;
	free(iter);

	ResortIndices(root);

	Size--;
}

template<typename T>
inline void List<T>::Extend(T Datas[], int ArraySize)
{
	iter = tail;

	for (int i = 0; i < ArraySize; i++)
	{
		iter->Next = InitNode(Datas[i], Size - 1, iter, nullptr);
		iter = iter->Next;
		tail = iter;
		tail->Next = nullptr;
		Size++;
	}
	Size--;
}

template<typename T>
inline void List<T>::Extend(int index, T Datas[], int ArraySize)
{
	iter = root;

	while (iter->Index != index)
		iter = iter->Next;


	for (int i = 0; i < ArraySize; i++)
	{
		iter->Next = InitNode(Datas[i], Size - 1, iter, nullptr);
		iter = iter->Next;
		tail = iter;
		tail->Next = nullptr;
		Size++;
	}
	Size--;
}

template<typename T>
inline void List<T>::Clear()
{
	while (Size != 0)
		Pop();
}

template<typename T>
inline int List<T>::IndexOf(T Data)
{
	iter = root;

	if (Size == 0)
		return -1;

	while (iter->Data != Data)
	{
		if (iter != tail)
			iter = iter->Next;
		else
			return -1;
	}

	return iter->Index;
}

template<typename T>
inline T* List<T>::ToArray()
{
	T* arr = new T[Size];
	iter = root;

	for (int i = 0; i < Size; i++)
	{
		arr[i] = iter->Data;
		iter = iter->Next;
	}
}

template<typename T>
inline void List<T>::Print()
{
	iter = root;
	if (Size == 0)
		std::cout << "The list is empty!" << std::endl;
	else
	{
		std::cout << "**----------------------------------------**" << std::endl;
		while (iter != tail)
		{
			std::cout << iter->Data << " i: " << iter->Index << std::endl;
			iter = iter->Next;
		}
		std::cout << iter->Data << " i: " << iter->Index << std::endl;
		std::cout << "**----------------------------------------**" << std::endl;
	}

}

#endif // !define _LIST_H_