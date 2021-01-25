#include <unistd.h>
#include <string.h>
#include <pthread.h>

typedef char ALIGN[16];

union header 
{
	struct 
	{
		size_t size;
		unsigned is_free;
		union header *next;
	} s;
	ALIGN stub;
};
typedef union header header_t;

header_t *head = NULL, *tail = NULL;
pthread_mutex_t global_mallock;

header_t *get_free_block(size_t size)
{
	header_t *curr = head;
	while (curr)
	{
		if (curr->s.is_free && curr->s.size >= size)
			return curr;
		curr = curr->s.next;
	}
	
	return NULL;
}
void *malloc(size_t size) 
{
	size_t total;
	void *block;
	header_t *header;

	if (!size)
		return NULL;

	pthread_mutex_lock(&global_mallock);
	header = get_free_block(size);
	
	if (header) 
	{
		header->s.is_free = 0;
		pthread_mutex_unlock(&global_mallock);
		return (void*) (header+1);
	
	}

	total = sizeof(header_t) + size;
	block = sbrk(total);
	if (block == (void* ) -1)
	{
		pthread_mutex_unlock(&global_mallock);
		return NULL;
	}
	
	header = block;
	header->s.size = size;
	header->s.is_free = 0;
	if (!head)
		head = header;
	
	if (tail)
		tail->s.next = header;

	tail = header;
	pthread_mutex_unlock(&global_mallock);
	return (void*)(header + 1);	
}



void free(void *block)
{	
	header_t *header, *tmp;
	void *pbreak;

	if (!block)
		return;

	pthread_mutex_lock(&global_mallock);
	header = (header_t*)block -1;

	pbreak = sbrk(0);

	if ((char*)block + header->s.size == pbreak) 
	{
		if (head == tail) 
		{
			head = tail = NULL;
		} else
		{
			tmp = head;
			while (tmp) 
			{
				if (tmp->s.next == tail) 
				{
					tmp->s.next = NULL;
					tail = tmp;
				}
				tmp = tmp->s.next;
			}
		}
		sbrk(0 - sizeof(header_t) - header->s.size);
		pthread_mutex_unlock(&global_mallock);
		return;
	}
	header->s.is_free = 1;
	pthread_mutex_unlock(&global_mallock);
}

void *calloc(size_t num, size_t nsize)
{
	size_t size;
	void *block;
	if (!num || !nsize) 
		return NULL;

	size = num * nsize;
	if (nsize != size / num)
		return NULL; // Checks for overflow
	
	block = malloc(size);
	if (!block)
		return NULL;
	memset(block, 0, size);
	return block;
}

void *realloc(void *block, size_t size)
{
	header_t *header;
	void *ret;

	if (!block || !size)
		return malloc(size);

	header = (header_t*)block - 1;

	if (header->s.size >= size)
		return block;

	ret = malloc(size);
	if (ret) 
	{
		memcpy(ret, block, header->s.size);
		free(block);
	}
	return ret;
}
