#pragma once
#ifndef _PECOFF_RELOCATABLE_H_
#define _PECOFF_RELOCATABLE_H_

#pragma warning( disable : 4244)	// signed/unsigned

#include <cinttypes>
#include <string>
#include <Windows.h>
#include <unordered_set>

class Relocatable
{
public:
	Relocatable() : Relocatable(0) {};
	Relocatable(size_t szData, IMAGE_SECTION_HEADER& section) : Relocatable(szData, section.VirtualAddress, section.PointerToRawData) {}
	Relocatable(size_t szData, uintptr_t desiredRelativeVirtualAddress = 0, uintptr_t desiredFileOffset = 0)
		: Relocatable(new uint8_t[szData], szData, desiredRelativeVirtualAddress, desiredFileOffset) {}
	Relocatable(void* pData, size_t szData, uintptr_t desiredRelativeVirtualAddress = 0, uintptr_t desiredFileOffset = 0)
	{
		p = pData;
		this->szData = szData;
		rva_desired = desiredRelativeVirtualAddress;
		fo_desired = desiredFileOffset;
	}
	~Relocatable() { delete p; }

public:
	Relocatable(const Relocatable&) = delete;
	Relocatable& operator=(const Relocatable&) = delete;

public:
	void* get() { return p; }
	size_t size() const { return szData; }
	uintptr_t rva() const { return rva_desired; }
	uintptr_t fo() const { return fo_desired; }

public:
	virtual enum class reloc_type { RVA, RVA32, FO, FO32 };

	void add_reloc(reloc_type type, uintptr_t relativeOffset) { relocs.push_back(std::make_pair(type, relativeOffset)); }
	void add_reloc(reloc_type type, void* pData) { add_reloc(type, (uintptr_t)pData - (uintptr_t)get()); }
	void add_reloc_rva(void* pData) { add_reloc(reloc_type::RVA, pData); }
	void add_reloc_fo(void* pData) { add_reloc(reloc_type::FO, pData); }

	void applyTo(int64_t actualRVA, int64_t actualFO)  { apply(actualRVA - rva_desired, actualFO - fo_desired); }
	virtual void apply(int64_t rva_shift, int64_t fo_shift)
	{
		std::unordered_set<uintptr_t> overlap_chk;

		for (auto &&i : relocs)
		{
			if (!overlap_chk.insert(i.second).second)
				throw std::exception("overlapping relocation found");

			switch (i.first)
			{
			case reloc_type::RVA32:
				*(uint32_t*)((uintptr_t)p + i.second) += rva_shift;
				break;
			case reloc_type::RVA:
				*(uintptr_t*)((uintptr_t)p + i.second) += rva_shift;
				break;
			case reloc_type::FO32:
				*(uint32_t*)((uintptr_t)p + i.second) += fo_shift;
				break;
			case reloc_type::FO:
				*(uintptr_t*)((uintptr_t)p + i.second) += fo_shift;
				break;
			}
		}
	}
	
	void shiftTo(int64_t actualRVA, int64_t actualFO) { shift(actualRVA - rva_desired, actualFO - fo_desired); }
	virtual void shift(int64_t rva_shift = 0, int64_t fo_shift = 0)
	{
		std::unordered_set<uintptr_t> overlap_chk;

		for (auto &&i : relocs)
		{
			if (!overlap_chk.insert(i.second).second)
				throw std::exception("overlapping relocation found");

			switch (i.first)
			{
			case reloc_type::RVA32:
				i.second += rva_shift;
				break;
			case reloc_type::RVA:
				i.second += rva_shift;
				break;
			case reloc_type::FO32:
				i.second += fo_shift;
				break;
			case reloc_type::FO:
				i.second += fo_shift;
				break;
			}
		}

		rva_desired += rva_shift;
		fo_desired += fo_shift;
	}
	
	void append(Relocatable& other) { insert(other, size()); }
	virtual void insert(Relocatable& other, int64_t offset)
	{
		size_t szExt = other.size();
		resize(size() + szExt);
		memmove((void*)((uintptr_t)get() + offset + szExt), (void*)((uintptr_t)get() + offset), size() - szExt - offset);
		auto drva = other.rva(), dfo = other.fo();
		other.applyTo(rva() + offset, fo() + offset);
		other.shiftTo(rva() + offset, fo() + offset);
		memcpy((void*)((uintptr_t)get() + offset), other.get(), other.size());
		for (auto &i : relocs)
			if (i.second > offset) i.second += szExt;
		for (auto &i : other.relocs)
			relocs.push_back(i);
		/* 'other' is restored */
		other.shiftTo(drva, dfo);
		other.applyTo(drva, dfo);
	}

protected:
	uintptr_t rva_desired;	// relative virtual address
	uintptr_t fo_desired;	// file offset
	std::vector<std::pair<reloc_type, uintptr_t>> relocs;
	virtual void resize(size_t newSize)
	{
		p = realloc(p, newSize);
		szData = newSize;
	}

private:
	void* p;
	size_t szData;

};

#endif
