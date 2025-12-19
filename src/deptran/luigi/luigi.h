#pragma once

#include "rrr.hpp"

#include <errno.h>


namespace janus {

class LuigiService: public rrr::Service {
public:
    enum {
        DISPATCH = 0x5f34f6b1,
        STATUSCHECK = 0x3bbb180a,
        OWDPING = 0x2e673c04,
        DEADLINEPROPOSE = 0x6fdf5c7d,
        DEADLINECONFIRM = 0x19423506,
        WATERMARKEXCHANGE = 0x66383d38,
    };
    int __reg_to__(rrr::Server* svr) {
        int ret = 0;
        if ((ret = svr->reg(DISPATCH, this, &LuigiService::__Dispatch__wrapper__)) != 0) {
            goto err;
        }
        if ((ret = svr->reg(STATUSCHECK, this, &LuigiService::__StatusCheck__wrapper__)) != 0) {
            goto err;
        }
        if ((ret = svr->reg(OWDPING, this, &LuigiService::__OwdPing__wrapper__)) != 0) {
            goto err;
        }
        if ((ret = svr->reg(DEADLINEPROPOSE, this, &LuigiService::__DeadlinePropose__wrapper__)) != 0) {
            goto err;
        }
        if ((ret = svr->reg(DEADLINECONFIRM, this, &LuigiService::__DeadlineConfirm__wrapper__)) != 0) {
            goto err;
        }
        if ((ret = svr->reg(WATERMARKEXCHANGE, this, &LuigiService::__WatermarkExchange__wrapper__)) != 0) {
            goto err;
        }
        return 0;
    err:
        svr->unreg(DISPATCH);
        svr->unreg(STATUSCHECK);
        svr->unreg(OWDPING);
        svr->unreg(DEADLINEPROPOSE);
        svr->unreg(DEADLINECONFIRM);
        svr->unreg(WATERMARKEXCHANGE);
        return ret;
    }
    // these RPC handler functions need to be implemented by user
    // for 'raw' handlers, req is rusty::Box (auto-cleaned); weak_sconn requires lock() before use
    virtual void Dispatch(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, const rrr::i64& expected_time, const rrr::i32& worker_id, const rrr::i32& num_ops, const rrr::i32& num_involved_shards, const std::vector<rrr::i32>& involved_shards, const std::string& ops_data, rrr::i32* req_nr_out, rrr::i64* txn_id_out, rrr::i32* status, rrr::i64* commit_timestamp, rrr::i32* num_results, std::string* results_data, rrr::DeferredReply* defer) = 0;
    virtual void StatusCheck(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, rrr::i32* req_nr_out, rrr::i64* txn_id_out, rrr::i32* status, rrr::i64* commit_timestamp, rrr::i32* num_results, std::string* results_data, rrr::DeferredReply* defer) = 0;
    virtual void OwdPing(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& send_time, rrr::i32* req_nr_out, rrr::i32* status, rrr::DeferredReply* defer) = 0;
    virtual void DeadlinePropose(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i64& proposed_ts, const rrr::i32& src_shard, const rrr::i32& phase, rrr::i32* req_nr_out, rrr::i64* tid_out, rrr::i64* proposed_ts_out, rrr::i32* shard_id, rrr::i32* status, rrr::DeferredReply* defer) = 0;
    virtual void DeadlineConfirm(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i32& src_shard, const rrr::i64& new_ts, rrr::i32* req_nr_out, rrr::i32* status, rrr::DeferredReply* defer) = 0;
    virtual void WatermarkExchange(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i32& src_shard, const rrr::i32& num_watermarks, const std::vector<rrr::i64>& watermarks, rrr::i32* req_nr_out, rrr::i32* status, rrr::DeferredReply* defer) = 0;
private:
    void __Dispatch__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i64* in_2 = new rrr::i64;
        req->m >> *in_2;
        rrr::i64* in_3 = new rrr::i64;
        req->m >> *in_3;
        rrr::i32* in_4 = new rrr::i32;
        req->m >> *in_4;
        rrr::i32* in_5 = new rrr::i32;
        req->m >> *in_5;
        rrr::i32* in_6 = new rrr::i32;
        req->m >> *in_6;
        std::vector<rrr::i32>* in_7 = new std::vector<rrr::i32>;
        req->m >> *in_7;
        std::string* in_8 = new std::string;
        req->m >> *in_8;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i64* out_1 = new rrr::i64;
        rrr::i32* out_2 = new rrr::i32;
        rrr::i64* out_3 = new rrr::i64;
        rrr::i32* out_4 = new rrr::i32;
        std::string* out_5 = new std::string;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_2;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_3;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_4;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_5;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete in_3;
            delete in_4;
            delete in_5;
            delete in_6;
            delete in_7;
            delete in_8;
            delete out_0;
            delete out_1;
            delete out_2;
            delete out_3;
            delete out_4;
            delete out_5;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->Dispatch(*in_0, *in_1, *in_2, *in_3, *in_4, *in_5, *in_6, *in_7, *in_8, out_0, out_1, out_2, out_3, out_4, out_5, __defer__);
    }
    void __StatusCheck__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i64* in_2 = new rrr::i64;
        req->m >> *in_2;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i64* out_1 = new rrr::i64;
        rrr::i32* out_2 = new rrr::i32;
        rrr::i64* out_3 = new rrr::i64;
        rrr::i32* out_4 = new rrr::i32;
        std::string* out_5 = new std::string;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_2;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_3;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_4;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_5;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete out_0;
            delete out_1;
            delete out_2;
            delete out_3;
            delete out_4;
            delete out_5;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->StatusCheck(*in_0, *in_1, *in_2, out_0, out_1, out_2, out_3, out_4, out_5, __defer__);
    }
    void __OwdPing__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i64* in_2 = new rrr::i64;
        req->m >> *in_2;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i32* out_1 = new rrr::i32;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete out_0;
            delete out_1;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->OwdPing(*in_0, *in_1, *in_2, out_0, out_1, __defer__);
    }
    void __DeadlinePropose__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i64* in_2 = new rrr::i64;
        req->m >> *in_2;
        rrr::i64* in_3 = new rrr::i64;
        req->m >> *in_3;
        rrr::i32* in_4 = new rrr::i32;
        req->m >> *in_4;
        rrr::i32* in_5 = new rrr::i32;
        req->m >> *in_5;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i64* out_1 = new rrr::i64;
        rrr::i64* out_2 = new rrr::i64;
        rrr::i32* out_3 = new rrr::i32;
        rrr::i32* out_4 = new rrr::i32;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_2;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_3;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_4;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete in_3;
            delete in_4;
            delete in_5;
            delete out_0;
            delete out_1;
            delete out_2;
            delete out_3;
            delete out_4;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->DeadlinePropose(*in_0, *in_1, *in_2, *in_3, *in_4, *in_5, out_0, out_1, out_2, out_3, out_4, __defer__);
    }
    void __DeadlineConfirm__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i64* in_2 = new rrr::i64;
        req->m >> *in_2;
        rrr::i32* in_3 = new rrr::i32;
        req->m >> *in_3;
        rrr::i64* in_4 = new rrr::i64;
        req->m >> *in_4;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i32* out_1 = new rrr::i32;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete in_3;
            delete in_4;
            delete out_0;
            delete out_1;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->DeadlineConfirm(*in_0, *in_1, *in_2, *in_3, *in_4, out_0, out_1, __defer__);
    }
    void __WatermarkExchange__wrapper__(rusty::Box<rrr::Request> req, rrr::WeakServerConnection weak_sconn) {
        rrr::i32* in_0 = new rrr::i32;
        req->m >> *in_0;
        rrr::i32* in_1 = new rrr::i32;
        req->m >> *in_1;
        rrr::i32* in_2 = new rrr::i32;
        req->m >> *in_2;
        rrr::i32* in_3 = new rrr::i32;
        req->m >> *in_3;
        std::vector<rrr::i64>* in_4 = new std::vector<rrr::i64>;
        req->m >> *in_4;
        rrr::i32* out_0 = new rrr::i32;
        rrr::i32* out_1 = new rrr::i32;
        auto __marshal_reply__ = [=] {
            auto sconn_opt = weak_sconn.upgrade();
            if (sconn_opt.is_some()) {
                auto sconn = sconn_opt.unwrap();
                const_cast<rrr::ServerConnection&>(*sconn) << *out_0;
                const_cast<rrr::ServerConnection&>(*sconn) << *out_1;
            }
        };
        auto __cleanup__ = [=] {
            delete in_0;
            delete in_1;
            delete in_2;
            delete in_3;
            delete in_4;
            delete out_0;
            delete out_1;
        };
        rrr::DeferredReply* __defer__ = new rrr::DeferredReply(std::move(req), weak_sconn, __marshal_reply__, __cleanup__);
        this->WatermarkExchange(*in_0, *in_1, *in_2, *in_3, *in_4, out_0, out_1, __defer__);
    }
};

class LuigiProxy {
protected:
    rrr::Client* __cl__;
public:
    LuigiProxy(rrr::Client* cl): __cl__(cl) { }
    rrr::FutureResult async_Dispatch(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, const rrr::i64& expected_time, const rrr::i32& worker_id, const rrr::i32& num_ops, const rrr::i32& num_involved_shards, const std::vector<rrr::i32>& involved_shards, const std::string& ops_data, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::DISPATCH, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << txn_id;
        *__cl__ << expected_time;
        *__cl__ << worker_id;
        *__cl__ << num_ops;
        *__cl__ << num_involved_shards;
        *__cl__ << involved_shards;
        *__cl__ << ops_data;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 Dispatch(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, const rrr::i64& expected_time, const rrr::i32& worker_id, const rrr::i32& num_ops, const rrr::i32& num_involved_shards, const std::vector<rrr::i32>& involved_shards, const std::string& ops_data, rrr::i32* req_nr_out, rrr::i64* txn_id_out, rrr::i32* status, rrr::i64* commit_timestamp, rrr::i32* num_results, std::string* results_data) {
        auto __fu_result__ = this->async_Dispatch(target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *txn_id_out;
            __fu__->get_reply() >> *status;
            __fu__->get_reply() >> *commit_timestamp;
            __fu__->get_reply() >> *num_results;
            __fu__->get_reply() >> *results_data;
        }
        // Arc auto-released
        return __ret__;
    }
    rrr::FutureResult async_StatusCheck(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::STATUSCHECK, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << txn_id;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 StatusCheck(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& txn_id, rrr::i32* req_nr_out, rrr::i64* txn_id_out, rrr::i32* status, rrr::i64* commit_timestamp, rrr::i32* num_results, std::string* results_data) {
        auto __fu_result__ = this->async_StatusCheck(target_server_id, req_nr, txn_id);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *txn_id_out;
            __fu__->get_reply() >> *status;
            __fu__->get_reply() >> *commit_timestamp;
            __fu__->get_reply() >> *num_results;
            __fu__->get_reply() >> *results_data;
        }
        // Arc auto-released
        return __ret__;
    }
    rrr::FutureResult async_OwdPing(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& send_time, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::OWDPING, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << send_time;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 OwdPing(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& send_time, rrr::i32* req_nr_out, rrr::i32* status) {
        auto __fu_result__ = this->async_OwdPing(target_server_id, req_nr, send_time);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *status;
        }
        // Arc auto-released
        return __ret__;
    }
    rrr::FutureResult async_DeadlinePropose(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i64& proposed_ts, const rrr::i32& src_shard, const rrr::i32& phase, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::DEADLINEPROPOSE, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << tid;
        *__cl__ << proposed_ts;
        *__cl__ << src_shard;
        *__cl__ << phase;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 DeadlinePropose(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i64& proposed_ts, const rrr::i32& src_shard, const rrr::i32& phase, rrr::i32* req_nr_out, rrr::i64* tid_out, rrr::i64* proposed_ts_out, rrr::i32* shard_id, rrr::i32* status) {
        auto __fu_result__ = this->async_DeadlinePropose(target_server_id, req_nr, tid, proposed_ts, src_shard, phase);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *tid_out;
            __fu__->get_reply() >> *proposed_ts_out;
            __fu__->get_reply() >> *shard_id;
            __fu__->get_reply() >> *status;
        }
        // Arc auto-released
        return __ret__;
    }
    rrr::FutureResult async_DeadlineConfirm(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i32& src_shard, const rrr::i64& new_ts, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::DEADLINECONFIRM, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << tid;
        *__cl__ << src_shard;
        *__cl__ << new_ts;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 DeadlineConfirm(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i64& tid, const rrr::i32& src_shard, const rrr::i64& new_ts, rrr::i32* req_nr_out, rrr::i32* status) {
        auto __fu_result__ = this->async_DeadlineConfirm(target_server_id, req_nr, tid, src_shard, new_ts);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *status;
        }
        // Arc auto-released
        return __ret__;
    }
    rrr::FutureResult async_WatermarkExchange(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i32& src_shard, const rrr::i32& num_watermarks, const std::vector<rrr::i64>& watermarks, const rrr::FutureAttr& __fu_attr__ = rrr::FutureAttr()) {
        auto __fu_result__ = __cl__->begin_request(LuigiService::WATERMARKEXCHANGE, __fu_attr__);
        if (__fu_result__.is_err()) {
            return __fu_result__;  // Propagate error
        }
        auto __fu__ = __fu_result__.unwrap();
        *__cl__ << target_server_id;
        *__cl__ << req_nr;
        *__cl__ << src_shard;
        *__cl__ << num_watermarks;
        *__cl__ << watermarks;
        __cl__->end_request();
        return rrr::FutureResult::Ok(__fu__);
    }
    rrr::i32 WatermarkExchange(const rrr::i32& target_server_id, const rrr::i32& req_nr, const rrr::i32& src_shard, const rrr::i32& num_watermarks, const std::vector<rrr::i64>& watermarks, rrr::i32* req_nr_out, rrr::i32* status) {
        auto __fu_result__ = this->async_WatermarkExchange(target_server_id, req_nr, src_shard, num_watermarks, watermarks);
        if (__fu_result__.is_err()) {
            return __fu_result__.unwrap_err();  // Return error code
        }
        auto __fu__ = __fu_result__.unwrap();
        rrr::i32 __ret__ = __fu__->get_error_code();
        if (__ret__ == 0) {
            __fu__->get_reply() >> *req_nr_out;
            __fu__->get_reply() >> *status;
        }
        // Arc auto-released
        return __ret__;
    }
};

} // namespace janus



