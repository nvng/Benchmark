#pragma once

#include <vector>

template <typename T, typename M>
class StateBase
{
public:
	explicit StateBase(int stateType) : mStateType(stateType) {}
	virtual ~StateBase() {}

	inline int GetStateType() const { return mStateType; }

	virtual void Enter(T& owner, M& evt) {}
	virtual void Exit(T& owner, M& evt) {}
	virtual void OnEvent(T& owner, M& evt) {}

private :
	void Update(const T& owner); // 禁止使用Update
	int mStateType;
};

template <typename T, typename M>
class FSMBase
{
public:
	typedef StateBase<T, M> StateBaseType;

	explicit FSMBase(int stateListSize)
		: mPreState(nullptr),
		mCurState(nullptr),
		mStateListSize(stateListSize)
	{
		mStateList.reserve(mStateListSize);
		for (int i = 0; i < mStateListSize; ++i)
			mStateList.push_back(nullptr);
	}

	virtual ~FSMBase()
	{
		for (auto& val : mStateList)
			delete val;
	}

	virtual void Init()
	{
		for (int i = 0; i < mStateListSize; ++i)
			mStateList[i] = CreateStateByType(i);
	}

	inline StateBaseType* GetCurState() { return mCurState; }
	inline StateBaseType* GetPreState() { return mPreState; }

	inline void OnEvent(T& obj, M& evt)
	{
		if (nullptr != mCurState)
			mCurState->OnEvent(obj, evt);
	}

	inline void ChangeState(int stateType, T& obj, M& evt)
	{
		StateBaseType* nextState = GetStateByType(stateType);
		if (nullptr == nextState)
			return;

		if (nullptr != mCurState)
			mCurState->Exit(obj, evt);

                mPreState = mCurState;
                mCurState = nextState;

		nextState->Enter(obj, evt);
	}

private :
	void Update(float deltaTime); // 禁止使用Update
	inline StateBaseType* GetStateByType(int stateType)
	{
		if (stateType >= 0 && stateType < mStateListSize)
			return mStateList[stateType];
		return nullptr;
	}

public :
	bool SetCurState(int stateType, T& obj, M& evt, bool isEnter)
	{
		StateBaseType* state = GetStateByType(stateType);
		if (nullptr == state)
			return false;

		if (nullptr != mCurState)
			mCurState->Exit(obj, evt);

		if (isEnter)
			state->Enter(obj, evt);

		mPreState = mCurState;
		mCurState = state;

		return true;
	}

protected:
	virtual StateBaseType* CreateStateByType(int stateType) = 0;

private:
	StateBaseType* mPreState;
	StateBaseType* mCurState;

	const int mStateListSize;
	std::vector<StateBaseType*> mStateList;
};

// vim: se enc=utf8 expandtab ts=8
