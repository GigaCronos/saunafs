#pragma once

#include <stdint.h>
#include <iostream>
#include <random>
#include <vector>

// Key: key_type
// Cmp_Fn: comparator struct
// P = prob_num / prob_den : Probability of the Bernoulli process to stop
//                           while choosing the level size of new nodes

template <typename Key, typename Cmp_Fn = std::less<Key>, uint32_t prob_num = 1,
          uint32_t prob_den = 2>
class SkipList {
public:
	using key_type = Key;
	using key_compare = Cmp_Fn;
	using reference = key_type &;
	using const_reference = const key_type &;
	using size_type = uint64_t;
	using difference_type = std::ptrdiff_t;

	static inline char updatePool[(sizeof(Key) + sizeof(size_type)) * 128];
	static constexpr size_t KeySize = sizeof(Key);
	static constexpr size_t PointerSize = sizeof(uint64_t);

	class SkipNodePool {
	public:
		// Constants
		static constexpr uint64_t kLevelBits = 7;
		static constexpr uint64_t kMaxLevel = (1 << kLevelBits);
		static constexpr uint64_t kBucketBits = 16;
		static constexpr uint64_t kBucketSize = (1 << kBucketBits);
		static constexpr uint64_t kOffsetBits = 64 - kLevelBits - kBucketBits;
		static constexpr uint64_t kOffsetSize = (uint64_t(1) << kOffsetBits);
		static constexpr double kFragmentationFactor = 0.999;
		SkipNodePool() {
			freeList_ = new uint64_t[kMaxLevel];
			memset(freeList_, 0, kMaxLevel * PointerSize);
			pool_ = new std::vector<char *>[kMaxLevel];
			fragmentationIndex_ = 1;
		}

		~SkipNodePool() {
			delete[] freeList_;
			for (uint64_t i = 0; i < kMaxLevel; i++) {
				for (auto bucket : pool_[i]) { delete[] bucket; }
			}
			delete[] pool_;
		}

		uint64_t alloc(int level) {
			if (!freeList_[level]) { alloc_new_bucket_(level); }
			uint64_t pointer = freeList_[level];
			freeList_[level] = *static_cast<uint64_t *>(get_address(pointer));
			return pointer;
		}

		void dealloc(uint64_t pt) {
			*static_cast<uint64_t *>(get_address(pt)) = freeList_[pt & (kMaxLevel - 1)];
			freeList_[pt & (kMaxLevel - 1)] = pt;
		}

		inline void *get_address(uint64_t pt) const {
			uint64_t level = pt & (kMaxLevel - 1);
			uint64_t bucketId = (pt >> kLevelBits) & (kOffsetSize - 1);
			uint64_t id = (pt >> (kLevelBits + kOffsetBits)) & (kBucketSize - 1);
			return pool_[level][bucketId] +
			       id * (KeySize + PointerSize + level * (PointerSize + sizeof(size_type)));
		}

	private:
		void alloc_new_bucket_(uint64_t level) {
			uint32_t levelBucketSize = kBucketSize / (level + 1) * fragmentationIndex_;
			fragmentationIndex_ =
			    std::max(fragmentationIndex_ * kFragmentationFactor, double(0.03));
			pool_[level].emplace_back(
			    new char[levelBucketSize *
			             (KeySize + PointerSize + level * (KeySize + sizeof(size_type)))]);
			for (int i = levelBucketSize - 1; i >= (level == 0 && pool_[level].size() == 1); i--) {
				dealloc(level | ((pool_[level].size() - 1) << kLevelBits) |
				        (uint64_t(i) << (kLevelBits + kOffsetBits)));
			}
		}
		double fragmentationIndex_;
		uint64_t *freeList_;
		std::vector<char *> *pool_;
	};

	static inline SkipNodePool nodePool;
	static inline std::mt19937_64 generator;
	static inline std::uniform_real_distribution<double> randDistribution;

	struct pointer;

	struct node {
		key_type key;
		pointer next_0;

		std::pair<pointer, size_type> *next() {
			return reinterpret_cast<std::pair<pointer, size_type> *>(
			    reinterpret_cast<char *>(this) + sizeof(node));
		}

		const std::pair<pointer, size_type> *next() const {
			return reinterpret_cast<const std::pair<pointer, size_type> *>(
			    reinterpret_cast<const char *>(this) + sizeof(node));
		}

		static pointer create(key_type key, size_t level) {
			pointer pt(nodePool.alloc(level));
			pt->key = key;
			pt->next_0 = pointer(0);
			// Inicializar el array next
			auto *next_arr = pt->next();
			for (size_t i = 0; i < level; i++) { next_arr[i] = {pointer(0), 0}; }
			return pt;
		}

		static void destroy(pointer pt) {
			if (pt != pointer(0)) {
				nodePool.dealloc(pt.address);  // Liberar la memoria
			}
		}
	};

	struct pointer {
	public:
		pointer() : address(0) {}

		explicit pointer(uint64_t _address) : address(_address) {}

		node &operator*() const { return *static_cast<node *>(nodePool.get_address(address)); }
		node *operator->() const { return static_cast<node *>(nodePool.get_address(address)); }

		bool operator==(const pointer &other) const { return address == other.address; }
		bool operator!=(const pointer &other) const { return !(*this == other); }

		uint64_t address;
	};

	class iterator {
	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = const Key;
		using difference_type = std::ptrdiff_t;
		using reference = const value_type &;

		iterator(pointer _ptr = pointer(0), size_type _idx = 0,
		         const SkipList<Key, Cmp_Fn, prob_num, prob_den> *_container = nullptr)
		    : node_ptr_(_ptr), idx_(_idx), container_(_container) {}
		key_type &operator*() const {
			if (idx_ < container_->size()) { return node_ptr_->key; }
			throw std::out_of_range("Invalid index");
		}
		key_type *operator->() const { return &**this; }
		iterator &operator++() {
			++idx_;
			if (idx_ > container_->size()) { throw std::out_of_range("Invalid Pointer"); }
			node_ptr_ = node_ptr_->next_0;
			return *this;
		}
		iterator operator++(int) {
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		bool operator==(const iterator &other) const {
			return node_ptr_ == other.node_ptr_ && idx_ == other.idx_;
		}
		bool operator!=(const iterator &other) const { return !(*this == other); }
		size_type get_index() const { return idx_; }

	private:
		pointer node_ptr_;
		size_t idx_;
		const SkipList<Key, Cmp_Fn, prob_num, prob_den> *container_;
	};

	using const_iterator = iterator;
	SkipList() {
		last_level_ = 0;
		size_ = 0;
		head_ = node::create(key_type(), 0);
		head_->next_0 = pointer(0);
	}
	~SkipList() {
		pointer temp = head_;
		pointer old = head_;
		while (temp != pointer(0)) {
			old = temp;
			temp = old->next_0;
			node::destroy(old);
		}
	}

	iterator begin() const { return iterator(this->head_->next_0, 0, this); }
	iterator end() const { return iterator(pointer(0), size_, this); }

	iterator lower_bound(const key_type &elem) {
		pointer current = head_;
		size_type sum = 0;
		{
			for (int level = last_level_; level > 0; level--) {
				while (current->next()[level - 1].first.address != 0 &&
				       !key_compare()(elem, current->next()[level - 1].first->key)) {
					sum += current->next()[level - 1].second;
					current = current->next()[level - 1].first;
				}
			}
			while (current->next_0.address != 0 && !key_compare()(elem, current->next_0->key)) {
				sum += 1;
				current = current->next_0;
			}
		}
		if (current == head_) { return this->begin(); }
		if (!key_compare()(elem, current->key) && !key_compare()(current->key, elem)) {
			return iterator(current, sum - 1, this);
		}
		sum += 1;
		current = current->next_0;
		if (current.address == 0) { return this->end(); }
		return iterator(current, sum - 1, this);
	}

	iterator find(const key_type &elem) {
		iterator current = lower_bound(elem);
		if (current != this->end() && !key_compare()(elem, *current) &&
		    !key_compare()(*current, elem)) {
			return current;
		} else {
			return this->end();
		}
	}

	std::pair<iterator, bool> insert(const key_type &elem) {
		pointer current = head_;
		size_type sum = 0;
		std::pair<pointer, size_type> *update =
		    reinterpret_cast<std::pair<pointer, size_type> *>(updatePool);
		{
			for (int level = last_level_; level > 0; level--) {
				while (current->next()[level - 1].first.address != 0 &&
				       !key_compare()(elem, current->next()[level - 1].first->key)) {
					sum += current->next()[level - 1].second;
					current = current->next()[level - 1].first;
				}
				update[level] = {current, sum};
			}
			while (current->next_0.address != 0 && !key_compare()(elem, current->next_0->key)) {
				sum += 1;
				current = current->next_0;
			}
			update[0] = {current, sum};
		}
		if (sum > 0 && !key_compare()(elem, current->key) && !key_compare()(current->key, elem)) {
			return {iterator(current, sum - 1, this), false};
		}

		uint32_t new_level = 0;
		while (randDistribution(generator) * prob_den > prob_num && new_level <= last_level_) {
			new_level++;
		}
		if (new_level == 128) { new_level = 127; }
		if (new_level > last_level_) {
			pointer new_head = node::create(head_->key, new_level);
			for (uint32_t level = 0; level < new_level; level++) {
				if (level) {
					new_head->next()[level - 1] = head_->next()[level - 1];
				} else {
					new_head->next_0 = head_->next_0;
				}
				if (update[level].first == head_) { update[level].first = new_head; }
			}
			update[new_level].first = new_head;
			update[new_level].second = 0;
			new_head->next()[new_level - 1] = {pointer(0), size_ + 1};
			node::destroy(head_);
			head_ = new_head;
			last_level_ = new_level;
		}

		pointer new_node = node::create(elem, new_level);
		{
			new_node->next_0 = update[0].first->next_0;
			update[0].first->next_0 = new_node;
			for (uint32_t level = 1; level <= last_level_; level++) {
				if (level <= new_level) {
					new_node->next()[level - 1].first =
					    update[level].first->next()[level - 1].first;
					new_node->next()[level - 1].second =
					    update[level].second + update[level].first->next()[level - 1].second + 1 -
					    (sum + 1);
					update[level].first->next()[level - 1].first = new_node;
					update[level].first->next()[level - 1].second = sum + 1 - update[level].second;
				} else {
					update[level].first->next()[level - 1].second++;
				}
			}
		}
		size_++;
		return {iterator(new_node, sum, this), true};
	}

	size_type erase(const key_type &elem) {
		pointer current = head_;
		size_type sum = 0;
		int del_level = -1;
		std::pair<pointer, size_type> *update =
		    reinterpret_cast<std::pair<pointer, size_type> *>(updatePool);
		{
			for (int level = last_level_; level > 0; level--) {
				while (current->next()[level - 1].first.address != 0 &&
				       key_compare()(current->next()[level - 1].first->key, elem)) {
					sum += current->next()[level - 1].second;
					current = current->next()[level - 1].first;
				}
				if (del_level == -1) {
					if (current->next()[level - 1].first.address != 0 &&
					    !key_compare()(elem, current->next()[level - 1].first->key)) {
						del_level = level;
					}
				}
				update[level] = {current, sum};
			}
			while (current->next_0.address != 0 && key_compare()(current->next_0->key, elem)) {
				sum += 1;
				current = current->next_0;
			}
			if (del_level == -1) {
				if (current->next_0.address != 0 && !key_compare()(elem, current->next_0->key)) {
					del_level = 0;
				}
			}
			update[0] = {current, sum};
		}
		if (del_level == -1) { return 0; }
		pointer target = current->next_0;
		{
			update[0].first->next_0 = target->next_0;
			for (uint32_t level = 1; level <= last_level_; level++) {
				if (int(level) <= del_level) {
					update[level].first->next()[level - 1].second =
					    sum + target->next()[level - 1].second - update[level].second;
					update[level].first->next()[level - 1].first = target->next()[level - 1].first;
				} else {
					update[level].first->next()[level - 1].second--;
				}
			}
		}
		node::destroy(target);
		size_--;
		return 1;
	}

	size_type erase(const iterator &it) {
		pointer current = head_;
		size_type sum = 0;
		int del_level = -1;
		std::pair<pointer, size_type> *update =
		    reinterpret_cast<std::pair<pointer, size_type> *>(updatePool);
		{
			for (int level = last_level_; level > 0; level--) {
				while (current->next()[level - 1].first.address != 0 &&
				       sum + current->next()[level - 1].second - 1 < it.get_index()) {
					sum += current->next()[level - 1].second;
					current = current->next()[level - 1].first;
				}
				if (del_level == -1) {
					if (current->next()[level - 1].first.address != 0 &&
					    sum + current->next()[level - 1].second - 1 == it.get_index()) {
						del_level = level;
					}
				}
				update[level] = {current, sum};
			}
			while (current->next_0.address != 0 && sum + 1 - 1 < it.get_index()) {
				sum += 1;
				current = current->next_0;
			}
			if (del_level == -1) {
				if (current->next_0.address != 0 && sum + 1 - 1 == it.get_index()) {
					del_level = 0;
				}
			}
			update[0] = {current, sum};
		}
		if (del_level == -1) { return 0; }
		pointer target = current->next_0;
		{
			update[0].first->next_0 = target->next_0;
			for (uint32_t level = 1; level <= last_level_; level++) {
				if (int(level) <= del_level) {
					update[level].first->next()[level - 1].second =
					    sum + target->next()[level - 1].second - update[level].second;
					update[level].first->next()[level - 1].first = target->next()[level - 1].first;
				} else {
					update[level].first->next()[level - 1].second--;
				}
			}
		}
		node::destroy(target);
		size_--;
		return 1;
	}

	iterator find_by_order(size_type nth) const {
		size_type sum = 0;
		pointer current = head_;
		{
			for (int level = last_level_; level > 0; level--) {
				if (current.address == 0) break;
				while (current.address != 0 && sum + current->next()[level - 1].second <= nth + 1) {
					sum += current->next()[level - 1].second;
					current = current->next()[level - 1].first;
				}
			}
			if (current.address != 0) {
				while (current.address != 0 && sum + 1 <= nth + 1) {
					sum += 1;
					current = current->next_0;
				}
			}
		}
		return iterator(current, sum - 1, this);
	}
	size_type order_of_key(const key_type &elem) {
		auto it = lower_bound(elem);
		return it.get_index();
	}

	size_type size() const { return size_; }

	bool empty() const { return size_ == 0; }

	bool _check_order() {
		auto it = this->begin();
		if (it == this->end()) { return true; }
		key_type K = *it;
		while (1) {
			it++;
			if (it == this->end()) { return true; }
			if (!key_compare()(K, *it)) { return false; }
			K = *it;
		}
		return true;
	}

private:
	pointer head_;
	size_type size_;
	uint8_t last_level_;
};