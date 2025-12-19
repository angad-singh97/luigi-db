import os
from simplerpc.marshal import Marshal
from simplerpc.future import Future

class LuigiService(object):
    DISPATCH = 0x5f34f6b1
    STATUSCHECK = 0x3bbb180a
    OWDPING = 0x2e673c04
    DEADLINEPROPOSE = 0x6fdf5c7d
    DEADLINECONFIRM = 0x19423506
    WATERMARKEXCHANGE = 0x66383d38

    __input_type_info__ = {
        'Dispatch': ['rrr::i32','rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32','rrr::i32','std::vector<rrr::i32>','std::string'],
        'StatusCheck': ['rrr::i32','rrr::i32','rrr::i64'],
        'OwdPing': ['rrr::i32','rrr::i32','rrr::i64'],
        'DeadlinePropose': ['rrr::i32','rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32'],
        'DeadlineConfirm': ['rrr::i32','rrr::i32','rrr::i64','rrr::i32','rrr::i64'],
        'WatermarkExchange': ['rrr::i32','rrr::i32','rrr::i32','rrr::i32','std::vector<rrr::i64>'],
    }

    __output_type_info__ = {
        'Dispatch': ['rrr::i32','rrr::i64','rrr::i32','rrr::i64','rrr::i32','std::string'],
        'StatusCheck': ['rrr::i32','rrr::i64','rrr::i32','rrr::i64','rrr::i32','std::string'],
        'OwdPing': ['rrr::i32','rrr::i32'],
        'DeadlinePropose': ['rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32'],
        'DeadlineConfirm': ['rrr::i32','rrr::i32'],
        'WatermarkExchange': ['rrr::i32','rrr::i32'],
    }

    def __bind_helper__(self, func):
        def f(*args):
            return getattr(self, func.__name__)(*args)
        return f

    def __reg_to__(self, server):
        server.__reg_func__(LuigiService.DISPATCH, self.__bind_helper__(self.Dispatch), ['rrr::i32','rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32','rrr::i32','std::vector<rrr::i32>','std::string'], ['rrr::i32','rrr::i64','rrr::i32','rrr::i64','rrr::i32','std::string'])
        server.__reg_func__(LuigiService.STATUSCHECK, self.__bind_helper__(self.StatusCheck), ['rrr::i32','rrr::i32','rrr::i64'], ['rrr::i32','rrr::i64','rrr::i32','rrr::i64','rrr::i32','std::string'])
        server.__reg_func__(LuigiService.OWDPING, self.__bind_helper__(self.OwdPing), ['rrr::i32','rrr::i32','rrr::i64'], ['rrr::i32','rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINEPROPOSE, self.__bind_helper__(self.DeadlinePropose), ['rrr::i32','rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32'], ['rrr::i32','rrr::i64','rrr::i64','rrr::i32','rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINECONFIRM, self.__bind_helper__(self.DeadlineConfirm), ['rrr::i32','rrr::i32','rrr::i64','rrr::i32','rrr::i64'], ['rrr::i32','rrr::i32'])
        server.__reg_func__(LuigiService.WATERMARKEXCHANGE, self.__bind_helper__(self.WatermarkExchange), ['rrr::i32','rrr::i32','rrr::i32','rrr::i32','std::vector<rrr::i64>'], ['rrr::i32','rrr::i32'])

    def Dispatch(__self__, target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data):
        raise NotImplementedError('subclass LuigiService and implement your own Dispatch function')

    def StatusCheck(__self__, target_server_id, req_nr, txn_id):
        raise NotImplementedError('subclass LuigiService and implement your own StatusCheck function')

    def OwdPing(__self__, target_server_id, req_nr, send_time):
        raise NotImplementedError('subclass LuigiService and implement your own OwdPing function')

    def DeadlinePropose(__self__, target_server_id, req_nr, tid, proposed_ts, src_shard, phase):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlinePropose function')

    def DeadlineConfirm(__self__, target_server_id, req_nr, tid, src_shard, new_ts):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlineConfirm function')

    def WatermarkExchange(__self__, target_server_id, req_nr, src_shard, num_watermarks, watermarks):
        raise NotImplementedError('subclass LuigiService and implement your own WatermarkExchange function')

class LuigiProxy(object):
    def __init__(self, clnt):
        self.__clnt__ = clnt

    def async_Dispatch(__self__, target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data):
        return __self__.__clnt__.async_call(LuigiService.DISPATCH, [target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data], LuigiService.__input_type_info__['Dispatch'], LuigiService.__output_type_info__['Dispatch'])

    def async_StatusCheck(__self__, target_server_id, req_nr, txn_id):
        return __self__.__clnt__.async_call(LuigiService.STATUSCHECK, [target_server_id, req_nr, txn_id], LuigiService.__input_type_info__['StatusCheck'], LuigiService.__output_type_info__['StatusCheck'])

    def async_OwdPing(__self__, target_server_id, req_nr, send_time):
        return __self__.__clnt__.async_call(LuigiService.OWDPING, [target_server_id, req_nr, send_time], LuigiService.__input_type_info__['OwdPing'], LuigiService.__output_type_info__['OwdPing'])

    def async_DeadlinePropose(__self__, target_server_id, req_nr, tid, proposed_ts, src_shard, phase):
        return __self__.__clnt__.async_call(LuigiService.DEADLINEPROPOSE, [target_server_id, req_nr, tid, proposed_ts, src_shard, phase], LuigiService.__input_type_info__['DeadlinePropose'], LuigiService.__output_type_info__['DeadlinePropose'])

    def async_DeadlineConfirm(__self__, target_server_id, req_nr, tid, src_shard, new_ts):
        return __self__.__clnt__.async_call(LuigiService.DEADLINECONFIRM, [target_server_id, req_nr, tid, src_shard, new_ts], LuigiService.__input_type_info__['DeadlineConfirm'], LuigiService.__output_type_info__['DeadlineConfirm'])

    def async_WatermarkExchange(__self__, target_server_id, req_nr, src_shard, num_watermarks, watermarks):
        return __self__.__clnt__.async_call(LuigiService.WATERMARKEXCHANGE, [target_server_id, req_nr, src_shard, num_watermarks, watermarks], LuigiService.__input_type_info__['WatermarkExchange'], LuigiService.__output_type_info__['WatermarkExchange'])

    def sync_Dispatch(__self__, target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DISPATCH, [target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops, num_involved_shards, involved_shards, ops_data], LuigiService.__input_type_info__['Dispatch'], LuigiService.__output_type_info__['Dispatch'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_StatusCheck(__self__, target_server_id, req_nr, txn_id):
        __result__ = __self__.__clnt__.sync_call(LuigiService.STATUSCHECK, [target_server_id, req_nr, txn_id], LuigiService.__input_type_info__['StatusCheck'], LuigiService.__output_type_info__['StatusCheck'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_OwdPing(__self__, target_server_id, req_nr, send_time):
        __result__ = __self__.__clnt__.sync_call(LuigiService.OWDPING, [target_server_id, req_nr, send_time], LuigiService.__input_type_info__['OwdPing'], LuigiService.__output_type_info__['OwdPing'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlinePropose(__self__, target_server_id, req_nr, tid, proposed_ts, src_shard, phase):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINEPROPOSE, [target_server_id, req_nr, tid, proposed_ts, src_shard, phase], LuigiService.__input_type_info__['DeadlinePropose'], LuigiService.__output_type_info__['DeadlinePropose'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlineConfirm(__self__, target_server_id, req_nr, tid, src_shard, new_ts):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINECONFIRM, [target_server_id, req_nr, tid, src_shard, new_ts], LuigiService.__input_type_info__['DeadlineConfirm'], LuigiService.__output_type_info__['DeadlineConfirm'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_WatermarkExchange(__self__, target_server_id, req_nr, src_shard, num_watermarks, watermarks):
        __result__ = __self__.__clnt__.sync_call(LuigiService.WATERMARKEXCHANGE, [target_server_id, req_nr, src_shard, num_watermarks, watermarks], LuigiService.__input_type_info__['WatermarkExchange'], LuigiService.__output_type_info__['WatermarkExchange'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

