import tfb_module_constructor from "../target/tfb.js";

const tfb_module=await tfb_module_constructor();
const tfb={};
export default tfb;

tfb.tfb_module=tfb_module;
tfb.module=tfb_module;
function def(name, ret, params) {
	tfb[name]=tfb_module.cwrap(name,ret,params);
}

def("tfb_create_controller","number",[]);
def("tfb_create_device","number",["number","number"]);
def("tfb_dispose","null",["number"]);
def("tfb_rx_push_byte","null",["number","number"]);
def("tfb_rx_is_available","boolean",["number"]);
def("tfb_set_id","null",["number","number"]);
def("tfb_message_func","null",["number","number"]);
def("tfb_message_from_func","null",["number","number"]);
def("tfb_millis_func","null",["number"]);
def("tfb_device_func","null",["number","number"]);
def("tfb_status_func","null",["number","number"]);
def("tfb_tx_is_available","boolean",["number"]);
def("tfb_tx_pop_byte","number",["number"]);
def("tfb_is_node","boolean",["number"]);
def("tfb_is_controller","boolean",["number"]);
def("tfb_is_connected","boolean",["number"]);
def("tfb_send","boolean",["number","number","number"]);
def("tfb_send_to","boolean",["number","number","number","number"]);
def("tfb_tick","null",["number"]);
def("tfb_get_queue_len","number",["number"]);
def("tfb_is_bus_available","boolean",["number"]);
def("tfb_srand","null",["number"]);
def("tfb_get_timeout","number",["number"]);
def("tfb_get_device_id_by_name","number",["number"]);
def("tfb_get_session_id","number",["number"]);
def("tfb_notify_bus_activity","null",["number"]);
def("tfb_get_errno","number",["number"]);

def("tfb_frame_create","number",["number"]);
def("tfb_frame_dispose","null",["number"]);
def("tfb_frame_get_size","number",["number"]);
def("tfb_frame_get_data","number",["number","number"]);
def("tfb_frame_get_data_size","number",["number","number"]);
def("tfb_frame_get_num","number",["number","number"]);
def("tfb_frame_get_buffer_at","number",["number","number"]);
def("tfb_frame_get_checksum","number",["number"]);
def("tfb_frame_has_data","boolean",["number","number"]);
def("tfb_frame_reset","null",["number"]);
def("tfb_frame_rx_push_byte","null",["number","number"]);
def("tfb_frame_rx_is_complete","boolean",["number"]);
def("tfb_frame_write_byte","null",["number","number"]);
def("tfb_frame_write_data","null",["number","number","number","number"]);
def("tfb_frame_write_num","null",["number","number","number"]);
def("tfb_frame_write_checksum","null",["number"]);
def("tfb_frame_tx_is_available","boolean",["number"]);
def("tfb_frame_tx_pop_byte","number",["number"]);
def("tfb_frame_tx_rewind","null",["number"]);
def("tfb_frame_get_key_at","number",["number","number"]);
def("tfb_frame_get_num_keys","number",["number"]);

tfb.tfb_millis_func(tfb.module.addFunction(()=>{
	return Date.now();
},"i"));

tfb.tfb_frame_get_buffer_array=(tfb_frame)=>{
	let a=[];

	for (let i=0; i<tfb.tfb_frame_get_size(tfb_frame); i++)
		a.push(tfb.tfb_frame_get_buffer_at(tfb_frame,i));

	return a;
}

tfb.TFB_CHECKSUM=1;
tfb.TFB_FROM=2;
tfb.TFB_TO=3;
tfb.TFB_PAYLOAD=4;
tfb.TFB_SEQ=5;
tfb.TFB_ACK=6;
tfb.TFB_SESSION_ID=10;
