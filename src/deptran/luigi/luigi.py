import os
from simplerpc.marshal import Marshal
from simplerpc.future import Future

class LuigiService(object):
    DISPATCH = 0x566b418a
    OWDPING = 0x17d2ba70
    DEADLINEPROPOSE = 0x1bfb4f4e
    DEADLINECONFIRM = 0x35905c6b
    DEADLINEBATCHPROPOSE = 0x438f7c65
    DEADLINEBATCHCONFIRM = 0x3890fc87
    WATERMARKEXCHANGE = 0x1a980d9d

    __input_type_info__ = {
        'Dispatch': ['rrr::i64','rrr::i64','rrr::i32','std::vector<rrr::i32>','std::string'],
        'OwdPing': ['rrr::i64'],
        'DeadlinePropose': ['rrr::i64','rrr::i32','rrr::i64'],
        'DeadlineConfirm': ['rrr::i64','rrr::i32','rrr::i64'],
        'DeadlineBatchPropose': ['std::vector<rrr::i64>','rrr::i32','std::vector<rrr::i64>'],
        'DeadlineBatchConfirm': ['std::vector<rrr::i64>','rrr::i32','std::vector<rrr::i64>'],
        'WatermarkExchange': ['rrr::i32','std::vector<rrr::i64>'],
    }

    __output_type_info__ = {
        'Dispatch': ['rrr::i32','rrr::i64','std::string'],
        'OwdPing': ['rrr::i32'],
        'DeadlinePropose': ['rrr::i32'],
        'DeadlineConfirm': ['rrr::i32'],
        'DeadlineBatchPropose': ['rrr::i32'],
        'DeadlineBatchConfirm': ['rrr::i32'],
        'WatermarkExchange': ['rrr::i32'],
    }

    def __bind_helper__(self, func):
        def f(*args):
            return getattr(self, func.__name__)(*args)
        return f

    def __reg_to__(self, server):
        server.__reg_func__(LuigiService.DISPATCH, self.__bind_helper__(self.Dispatch), ['rrr::i64','rrr::i64','rrr::i32','std::vector<rrr::i32>','std::string'], ['rrr::i32','rrr::i64','std::string'])
        server.__reg_func__(LuigiService.OWDPING, self.__bind_helper__(self.OwdPing), ['rrr::i64'], ['rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINEPROPOSE, self.__bind_helper__(self.DeadlinePropose), ['rrr::i64','rrr::i32','rrr::i64'], ['rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINECONFIRM, self.__bind_helper__(self.DeadlineConfirm), ['rrr::i64','rrr::i32','rrr::i64'], ['rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINEBATCHPROPOSE, self.__bind_helper__(self.DeadlineBatchPropose), ['std::vector<rrr::i64>','rrr::i32','std::vector<rrr::i64>'], ['rrr::i32'])
        server.__reg_func__(LuigiService.DEADLINEBATCHCONFIRM, self.__bind_helper__(self.DeadlineBatchConfirm), ['std::vector<rrr::i64>','rrr::i32','std::vector<rrr::i64>'], ['rrr::i32'])
        server.__reg_func__(LuigiService.WATERMARKEXCHANGE, self.__bind_helper__(self.WatermarkExchange), ['rrr::i32','std::vector<rrr::i64>'], ['rrr::i32'])

    def Dispatch(__self__, txn_id, expected_time, worker_id, involved_shards, ops_data):
        raise NotImplementedError('subclass LuigiService and implement your own Dispatch function')

    def OwdPing(__self__, send_time):
        raise NotImplementedError('subclass LuigiService and implement your own OwdPing function')

    def DeadlinePropose(__self__, tid, src_shard, proposed_ts):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlinePropose function')

    def DeadlineConfirm(__self__, tid, src_shard, agreed_ts):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlineConfirm function')

    def DeadlineBatchPropose(__self__, tids, src_shard, proposed_timestamps):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlineBatchPropose function')

    def DeadlineBatchConfirm(__self__, tids, src_shard, agreed_timestamps):
        raise NotImplementedError('subclass LuigiService and implement your own DeadlineBatchConfirm function')

    def WatermarkExchange(__self__, src_shard, watermarks):
        raise NotImplementedError('subclass LuigiService and implement your own WatermarkExchange function')

class LuigiProxy(object):
    def __init__(self, clnt):
        self.__clnt__ = clnt

    def async_Dispatch(__self__, txn_id, expected_time, worker_id, involved_shards, ops_data):
        return __self__.__clnt__.async_call(LuigiService.DISPATCH, [txn_id, expected_time, worker_id, involved_shards, ops_data], LuigiService.__input_type_info__['Dispatch'], LuigiService.__output_type_info__['Dispatch'])

    def async_OwdPing(__self__, send_time):
        return __self__.__clnt__.async_call(LuigiService.OWDPING, [send_time], LuigiService.__input_type_info__['OwdPing'], LuigiService.__output_type_info__['OwdPing'])

    def async_DeadlinePropose(__self__, tid, src_shard, proposed_ts):
        return __self__.__clnt__.async_call(LuigiService.DEADLINEPROPOSE, [tid, src_shard, proposed_ts], LuigiService.__input_type_info__['DeadlinePropose'], LuigiService.__output_type_info__['DeadlinePropose'])

    def async_DeadlineConfirm(__self__, tid, src_shard, agreed_ts):
        return __self__.__clnt__.async_call(LuigiService.DEADLINECONFIRM, [tid, src_shard, agreed_ts], LuigiService.__input_type_info__['DeadlineConfirm'], LuigiService.__output_type_info__['DeadlineConfirm'])

    def async_DeadlineBatchPropose(__self__, tids, src_shard, proposed_timestamps):
        return __self__.__clnt__.async_call(LuigiService.DEADLINEBATCHPROPOSE, [tids, src_shard, proposed_timestamps], LuigiService.__input_type_info__['DeadlineBatchPropose'], LuigiService.__output_type_info__['DeadlineBatchPropose'])

    def async_DeadlineBatchConfirm(__self__, tids, src_shard, agreed_timestamps):
        return __self__.__clnt__.async_call(LuigiService.DEADLINEBATCHCONFIRM, [tids, src_shard, agreed_timestamps], LuigiService.__input_type_info__['DeadlineBatchConfirm'], LuigiService.__output_type_info__['DeadlineBatchConfirm'])

    def async_WatermarkExchange(__self__, src_shard, watermarks):
        return __self__.__clnt__.async_call(LuigiService.WATERMARKEXCHANGE, [src_shard, watermarks], LuigiService.__input_type_info__['WatermarkExchange'], LuigiService.__output_type_info__['WatermarkExchange'])

    def sync_Dispatch(__self__, txn_id, expected_time, worker_id, involved_shards, ops_data):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DISPATCH, [txn_id, expected_time, worker_id, involved_shards, ops_data], LuigiService.__input_type_info__['Dispatch'], LuigiService.__output_type_info__['Dispatch'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_OwdPing(__self__, send_time):
        __result__ = __self__.__clnt__.sync_call(LuigiService.OWDPING, [send_time], LuigiService.__input_type_info__['OwdPing'], LuigiService.__output_type_info__['OwdPing'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlinePropose(__self__, tid, src_shard, proposed_ts):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINEPROPOSE, [tid, src_shard, proposed_ts], LuigiService.__input_type_info__['DeadlinePropose'], LuigiService.__output_type_info__['DeadlinePropose'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlineConfirm(__self__, tid, src_shard, agreed_ts):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINECONFIRM, [tid, src_shard, agreed_ts], LuigiService.__input_type_info__['DeadlineConfirm'], LuigiService.__output_type_info__['DeadlineConfirm'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlineBatchPropose(__self__, tids, src_shard, proposed_timestamps):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINEBATCHPROPOSE, [tids, src_shard, proposed_timestamps], LuigiService.__input_type_info__['DeadlineBatchPropose'], LuigiService.__output_type_info__['DeadlineBatchPropose'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_DeadlineBatchConfirm(__self__, tids, src_shard, agreed_timestamps):
        __result__ = __self__.__clnt__.sync_call(LuigiService.DEADLINEBATCHCONFIRM, [tids, src_shard, agreed_timestamps], LuigiService.__input_type_info__['DeadlineBatchConfirm'], LuigiService.__output_type_info__['DeadlineBatchConfirm'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

    def sync_WatermarkExchange(__self__, src_shard, watermarks):
        __result__ = __self__.__clnt__.sync_call(LuigiService.WATERMARKEXCHANGE, [src_shard, watermarks], LuigiService.__input_type_info__['WatermarkExchange'], LuigiService.__output_type_info__['WatermarkExchange'])
        if __result__[0] != 0:
            raise Exception("RPC returned non-zero error code %d: %s" % (__result__[0], os.strerror(__result__[0])))
        if len(__result__[1]) == 1:
            return __result__[1][0]
        elif len(__result__[1]) > 1:
            return __result__[1]

