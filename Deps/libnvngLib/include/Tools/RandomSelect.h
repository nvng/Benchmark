#pragma once

class RandomSelect final
{
public:
        typedef double(*RandInRangeFuncType)(double, double);
	explicit RandomSelect(RandInRangeFuncType randInRange);
	~RandomSelect();

	bool Add(double ratio, void* data);
	void* Get() const;
	void* Get(double ratio) const;
	double GetSum() const;

	void Clear();

private:
	class impl;
	impl* this_;
};

// vim: fenc=utf8:expandtab:ts=8:noma
