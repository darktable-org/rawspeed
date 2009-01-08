#ifndef SoftWire_Link_hpp
#define SoftWire_Link_hpp

namespace SoftWire
{
	template<class T>
	class Link : public T
	{
	public:
		Link();
		
		~Link();

		Link *append(const T &t);
		Link *next() const;
		Link *tail() const;

	private:
		Link *n;   // Next
		Link *t;   // Tail
	};
}

namespace SoftWire
{
	template<class T>
	Link<T>::Link()
	{
		n = 0;
		t = 0;
	}

	template<class T>
	Link<T>::~Link()
	{
		// Avoid call stack overflow
		while(n)
		{
			Link *nextNext = n->n;
			n->n = 0;
			delete n;
			n = nextNext;
		}

		t = 0;
	}

	template<class T>
	Link<T> *Link<T>::append(const T &e)
	{
		if(t == 0)   // Empty chain
		{
			*(T*)this = e;
			t = this;
		}
		else if(t != this)
		{
			t = t->append(e);
		}
		else
		{
			t = n = new Link();
			*(T*)t = e;
			t->t = t;
		}

		return t;
	}

	template<class T>
	Link<T> *Link<T>::next() const
	{
		return n;
	}

	template<class T>
	Link<T> *Link<T>::tail() const
	{
		return t;
	}
}

#endif   // SoftWire_Link_hpp
