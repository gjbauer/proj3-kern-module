
char *STRDUP(const char *str)
{
#ifdef _KERNEL
	return strdup(str, M_MYFS);
#else
	return strdup(str);
#endif
}

void FREE(void *addr)
{
#ifdef _KERNEL
	free(addr, M_MYFS);
#else
	free(addr);
#endif
}

void *MALLOC(size_t size)
{
#ifdef _KERNEL
	return malloc(size, M_MYFS, M_WAITOK | M_ZERO);
#else
	return malloc(size);
#endif
}
