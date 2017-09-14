#include <stdlib.h>
#include <stdio.h>

#include "maibu_sdk.h"
#include "maibu_res.h"



static enum GColor color1 = GColorRed;

//表盘的记录的当天日期，用于比较日期是否变为第二天
//static uint8_t g_today_num = 0;
static uint32_t realstep = 0, deltastep = 0 , speed = 0, pace = 0;
static uint32_t updatetime = 0;
static SportData last_data, data;
//static bool new_win_flag = true;
/*窗口句柄*/
static uint32_t g_window = -1;

/*GPS及海拔数据结构*/
static SGpsAltitude g_gps_altitude;

/*Web通讯ID*/
static uint32_t g_comm_id_web = 0;

/*Phone通讯ID*/
static uint32_t g_comm_id_gps = 0;

/*存放当前天气图标KEY*/
#define WEATHER_ICON_KEY	1
/*存放当前天气温度图标*/
#define WEATHER_TEMP_KEY	2

/*如果没有获取到天气图标，设置特殊天气图标*/
#define WEATHER_ICON_SPEC	0
/*如果没有获取到温度，设置特殊温度标志*/
#define WEATHER_TEMP_SPEC	125


/*获取天气数据云服务器地址*/
#define WEATHER_WEB     "http://api.openweathermap.org/data/2.5/weather?appid=6f884022975ba0c11422a597dec48ddf&units=metric&cnt=1&"

/*图片ID*/

/*no data*/
#define WEATHER_ICON_NO_DATA		0

/*clear sky*/
#define WEATHER_ICON_CLEAR_SKY		1

/*few clouds*/
#define WEATHER_ICON_FEW_CLOUDS		2

/*scattered clouds*/
#define WEATHER_ICON_SCATTERED_CLOUDS	3

/*broken clouds*/
#define WEATHER_ICON_BROKEN_CLOUDS	4

/*shower rain*/
#define WEATHER_ICON_SHOWER_RAIN	9

/*rain*/
#define WEATHER_ICON_RAIN		10

/*thunderstorm*/
#define WEATHER_ICON_THUNDERSTORM	11

/*snow*/
#define WEATHER_ICON_SNOW		13

/*mist*/
#define WEATHER_ICON_MIST		50


const static char wday_str[7][8] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
static P_Window init_watch(void);
void weather_init_store()
{
  /*创建保存天气图标文件*/
  app_persist_create(WEATHER_ICON_KEY, 1);

  /*先读取天气图标, 如果是第一次，设置默认值*/
  uint8_t icon_type = 0;
  int8_t icon_read_size = 0;
  icon_read_size = app_persist_read_data(WEATHER_ICON_KEY, 0, (unsigned char *) & icon_type, sizeof(int8_t));
  /*第一次使用, 显示默认值*/
  if (icon_read_size == 0)
    {
      icon_type = WEATHER_ICON_SPEC;
      app_persist_write_data_extend(WEATHER_ICON_KEY, (unsigned char *)&icon_type, sizeof(int8_t));
    }
  /*创建保存温度文件*/
  app_persist_create(WEATHER_TEMP_KEY, 1);
  /*先读取温度, 如果第一次，设置默认值*/
  int8_t temp = 0;
  int8_t temp_read_size = 0;
  temp_read_size = app_persist_read_data(WEATHER_TEMP_KEY, 0, (unsigned char *) & temp, sizeof(int8_t));
  if (temp_read_size == 0)
    {
      temp = WEATHER_TEMP_SPEC;
      app_persist_write_data_extend(WEATHER_TEMP_KEY, (unsigned char *)&temp, sizeof(int8_t));
    }
}

/*根据图标类型获取对应的天气图标*/
int32_t weather_get_icon_key(int32_t icon_type)
{

  int32_t type = RES_WEATHER_CLOUDY;
  if (icon_type == WEATHER_ICON_NO_DATA)
    {
      type = RES_WEATHER_NODATA;
    }
  else if (icon_type == WEATHER_ICON_CLEAR_SKY)
    {
      type = RES_WEATHER_SUNNY;
    }
  else if (icon_type == WEATHER_ICON_FEW_CLOUDS)
    {
      type = RES_WEATHER_CLOUDY;
    }
  else if (icon_type == WEATHER_ICON_SCATTERED_CLOUDS)
    {
      type = RES_WEATHER_CLOUDY;
    }
  else if (icon_type == WEATHER_ICON_BROKEN_CLOUDS)
    {
      type = RES_WEATHER_OVERCAST;
    }
  else if (icon_type == WEATHER_ICON_SHOWER_RAIN)
    {
      type = RES_WEATHER_RAIN_1;
    }
  else if (icon_type == WEATHER_ICON_RAIN)
    {
      type = RES_WEATHER_RAIN_2;
    }
  else if (icon_type == WEATHER_ICON_THUNDERSTORM)
    {
      type = RES_WEATHER_RAIN_3;
    }
  else if (icon_type == WEATHER_ICON_SNOW)
    {
      type = RES_WEATHER_SNOW_2;
    }
  else if (icon_type == WEATHER_ICON_MIST)
    {
      type = RES_WEATHER_FOG;
    }
  else
    {
      type = RES_WEATHER_CLOUDY;
    }
  return type;
}

//重新载入并刷新窗口所有图层
void window_reloading(void)
{
  /*根据窗口ID获取窗口句柄*/
  P_Window p_old_window = (P_Window) app_window_stack_get_window_by_id(g_window);
  if (NULL != p_old_window)
    {
      P_Window p_window = init_watch();
      if (NULL != p_window)
        {
          g_window = app_window_stack_replace_window(p_old_window, p_window);
        }
    }

}

/*
 *--------------------------------------------------------------------------------------
 *     function:  weather_request_web
 *    parameter:
 *       return:
 *  description:  请求网络数据
 * 	  other:
 *--------------------------------------------------------------------------------------
 */
void weather_request_web()
{
  /*拼接url请求地址, 注意url的缓存大小*/
  char url[200] = "";
  sprintf(url, "%slat=%s&lon=%s", WEATHER_WEB, g_gps_altitude.lat, g_gps_altitude.lon);
  /*拼接过滤参数，即只接受和过滤参数匹配的返回值*/
  char param[20] = "";
  sprintf(param, "%s,%s", "temp", "icon");
  /*15分钟同步一次天气数据*/
  g_comm_id_web = maibu_comm_request_web(url, param, 60 * 15);
}

/*
 *--------------------------------------------------------------------------------------
 *     function:  weather_phone_recv_callback
 *    parameter:
 *       return:
 *  description:  接受手机数据回调
 * 	  other:
 *--------------------------------------------------------------------------------------
 */
void weather_phone_recv_callback(enum ERequestPhone type, void *context)
{

  /*如果不是GPS海拔数据， 退出*/
  if (type != ERequestPhoneGPSAltitude)
    {
      return;
    }

  /*提取经度纬度*/
  memcpy(&g_gps_altitude, (SGpsAltitude *)context, sizeof(SGpsAltitude));

  /*请求Web获取天气数据*/
  weather_request_web();
}

/*
 *--------------------------------------------------------------------------------------
 *     function:
 *    parameter:
 *       return:
 *  description:  接受WEB数据回调
 * 	  other:
 *--------------------------------------------------------------------------------------
 */
void weather_web_recv_callback(const uint8_t *buff, uint16_t size)
{
  int8_t icon = 0;
  int tempera = 0 ;

  /*提取温度及对应的图片ID*/
  int32_t icon_tmp = 0, tempera_tmp = 0;
  if ((-1 == maibu_get_json_int(buff, "icon", &icon_tmp)) || (-1 == maibu_get_json_int(buff, "temp", &tempera_tmp)))
    {
      return;
    }
  icon = icon_tmp;
  tempera = tempera_tmp;

  app_persist_write_data_extend(WEATHER_ICON_KEY, (unsigned char *)&icon, sizeof(int8_t));
  app_persist_write_data_extend(WEATHER_TEMP_KEY, (unsigned char *)&tempera, sizeof(int8_t));

}


/*
 *--------------------------------------------------------------------------------------
 *     function:  watch_time_change
 *    parameter:
 *       return:
 *  description:  系统时间有变化时，更新时间图层
 * 	      other:
 *--------------------------------------------------------------------------------------
 */
static void watch_time_change(enum SysEventType type, void *context)
{
  /*时间更改*/
  if (type == SysEventTypeTimeChange)
    {

      window_reloading();
    }
}

void weather_comm_result_callback(enum ECommResult result, uint32_t comm_id, void *context)
{
  /*如果上一次请求GPS通讯失败，并且通讯ID相同，则重新发送*/
  if ((result == ECommResultFail) && (comm_id == g_comm_id_gps))
    {
      g_comm_id_gps = maibu_comm_request_phone(ERequestPhoneGPSAltitude,  NULL, 0);
    }

  /*如果上一次请求WEB通讯失败，并且通讯ID相同，则重新发送*/
  if ((result == ECommResultFail) && (comm_id == g_comm_id_web))
    {
      weather_request_web();
    }

}



/*
 *--------------------------------------------------------------------------------------
 *     function:  init_watch
 *    parameter:
 *       return:
 *  description:  生成表盘窗口
 * 	      other:
 *--------------------------------------------------------------------------------------
 */
static P_Window init_watch(void)
{
  P_Window p_window = NULL;
  P_Layer p_layer = NULL;

  //int a;

  maibu_get_sport_data(&data, 0);
  realstep = data.step;
  uint32_t updatetime1 = app_get_time_number();

  if (updatetime1 - updatetime == 60)
    {
      deltastep = data.step - last_data.step;
      if (deltastep >= 80)
        {
          pace = 1000000 / (data.distance - last_data.distance);
          speed = (data.distance - last_data.distance) * 60 / 10000;
        }
      last_data = data;
      updatetime = updatetime1;
    }
  else if (updatetime1 - updatetime > 60)
    {
      deltastep = 0;
      pace = 0;
      speed = 0;
      last_data = data;
      updatetime = updatetime1;
    }
  /* else
    if (!new_win_flag)
      {
        deltastep = 0;
        pace = 0;
        speed = 0;
        last_data = data;
      } */

  //if (new_win_flag) new_win_flag = false;



  /*时间图层，图片图层位置*/

  GRect timeframe = {{0, 20}, {50, 176}};
  GRect text1frame = {{0, 69}, {28, 176}};
  GRect iconframe = {{68, 96}, {40, 40}};

  GRect text2frame = {{0, 143}, {33, 15}};
  GRect text3frame = {{15, 136}, {20, 80}};
  GRect text4frame = {{15, 156}, {20, 55}};

  GRect text5frame = {{100, 143}, {33, 60}};
  GRect text6frame = {{160, 136}, {20, 16}};
  GRect text7frame = {{160, 156}, {20, 16}};
  char buff[30] = {0};

  /*创建一个窗口*/
  p_window = app_window_create();
  if (NULL == p_window) return NULL;
  struct date_time dt;
  app_service_get_datetime(&dt);
  enum GColor colors[] = {GColorRed, GColorGreen, GColorBlue, GColorPurple, GColorCyan}; //,GColorYellow
  color1 = colors[dt.min%5];

  uint16_t prog = realstep > 10000 ? 10000 : realstep;
  prog = prog * 176 / 10000;


  //绘制进度条
  Geometry *p_geometry_array[3];

  GPoint points[4] = { {0, 0}, {176, 0}, {176, 176}, {0, 176}};
  Polygon background   = {4, points};
  Geometry geometry = {GeometryTypePolygon, FillArea, GColorWhite, (void*)&background};
  p_geometry_array[0] = &geometry;

  GPoint prog_bar_points[4] = { {0, 0}, {prog, 0}, {prog, 7}, {0, 7}};
  Polygon prog_bar   = {4, prog_bar_points};
  Geometry geometry1 = {GeometryTypePolygon, FillArea, color1, (void*)&prog_bar};
  p_geometry_array[1] = &geometry1;

  /*直线*/
  Line l = {{0, 8}, {176, 8}};	//直线的起始点坐标

  /*几何结构体，依次为几何类型、填充类型、填充颜色、具体的几何图结构*/
  Geometry lg = {GeometryTypeLine, FillOutline, GColorBlack, (void*)&l};
  p_geometry_array[2] = &lg;	//存入指针数组中

  LayerGeometry prog_bar_struct = {3, p_geometry_array};

  p_layer = app_layer_create_geometry(&prog_bar_struct);
  app_window_add_layer(p_window, p_layer);

  //显示时间
  sprintf(buff, "%02d:%02d", dt.hour, dt.min);
  LayerText timetext = {buff, timeframe, GAlignCenter, GColorBlack, U_ASCII_ARIAL_42};//
  p_layer = app_layer_create_text(&timetext);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);

  //sprintf(buff, "%d", a);
  if (deltastep < 80)
    {
      SLunarData sld = {0};
      maibu_get_lunar_calendar(NULL, &sld);
      if (dt.min % 2)
        sprintf(buff, "%d-%d%s", dt.mon, dt.mday, wday_str[dt.wday]);
      else
        sprintf(buff, "%s%s", sld.mon, sld.day); //
    }
  else
    sprintf(buff, "%d/%dm", data.step, data.distance / 100);


  LayerText text1 = {buff, text1frame, GAlignCenter, color1, U_ASCII_ARIAL_20};//
  p_layer = app_layer_create_text(&text1);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);


  if (deltastep < 80)
    sprintf(buff, "%d", data.floor);
  else
    sprintf(buff, "%d.%d", speed / 10, speed % 10);
  if (strlen(buff) > 1)
    {
      text2frame.size.w = 15 * strlen(buff);
      uint8_t i;
      for (i = 0;i < strlen(buff);i++)
        if (buff[i] == '.')   text2frame.size.w = text2frame.size.w - 10;
      text3frame.origin.x = text2frame.size.w;
      text4frame.origin.x = text2frame.size.w;
    }

  LayerText text2 = {buff, text2frame, GAlignRight, GColorBlack, U_ASCII_ARIAL_24};//
  p_layer = app_layer_create_text(&text2);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);

  if (deltastep < 80)
    sprintf(buff, "%d", data.step);
  else
    sprintf(buff, "%d", deltastep);
  LayerText text3 = {buff, text3frame, GAlignLeft, GColorBlack, U_ASCII_ARIAL_20};//
  p_layer = app_layer_create_text(&text3);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);

  if (deltastep < 80)
    sprintf(buff, "steps");
  else
    sprintf(buff, "speed");
  LayerText text4 = {buff, text4frame, GAlignLeft, GColorWhite, U_ASCII_ARIAL_16};//
  p_layer = app_layer_create_text(&text4);
  app_layer_set_bg_color(p_layer, color1);
  app_window_add_layer(p_window, p_layer);

  if (deltastep < 80)
    {
      int8_t temp_int = 0;

      app_persist_read_data(WEATHER_TEMP_KEY, 0, (unsigned char *)&temp_int, sizeof(int8_t));

      if (temp_int == WEATHER_TEMP_SPEC)
        {
          strcpy(buff, "--");
        }
      else
        {
          sprintf(buff, "%d", temp_int);
        }
    }
  else
    sprintf(buff, "%d.%d", pace / 10, pace % 10);
  LayerText text5 = {buff, text5frame, GAlignRight, GColorBlack, U_ASCII_ARIAL_24};//
  p_layer = app_layer_create_text(&text5);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);

  if (deltastep < 80)
    sprintf(buff, "o");
  else
    sprintf(buff, "'");
  LayerText text6 = {buff, text6frame, GAlignLeft, GColorBlack, U_ASCII_ARIAL_16};//
  p_layer = app_layer_create_text(&text6);
  app_layer_set_bg_color(p_layer, GColorWhite);
  app_window_add_layer(p_window, p_layer);

  if (deltastep < 80)
    sprintf(buff, "C");
  else
    sprintf(buff, "p");
  LayerText text7 = {buff, text7frame, GAlignLeft, GColorWhite, U_ASCII_ARIAL_16};//
  p_layer = app_layer_create_text(&text7);
  app_layer_set_bg_color(p_layer, color1);
  app_window_add_layer(p_window, p_layer);

  /*添加天气图标*/

  app_persist_create(WEATHER_ICON_KEY, 1);


  /*读取天气图标*/
  int8_t icon_type = 0;

  app_persist_read_data(WEATHER_ICON_KEY, 0, (unsigned char *)&icon_type, sizeof(int8_t));

  /*如果icon_type大于0，显示天气图标*/
  if (icon_type >= 0)

    {
      /*添加天气图标*/

      GBitmap bitmap_icon;
      res_get_user_bitmap(weather_get_icon_key(icon_type), &bitmap_icon);
      LayerBitmap lb_icon = {bitmap_icon, iconframe, GAlignLeft};
      p_layer = app_layer_create_bitmap(&lb_icon);
      //app_layer_set_bg_color(p_layer, GColorBlack);
      if (p_layer != NULL)
        {
          app_window_add_layer(p_window, p_layer);
        }
    }


  return p_window;
}



/*
 *--------------------------------------------------------------------------------------
 *     function:  main
 *    parameter:
 *       return:
 *  description:  主程序
 * 	      other:
 *--------------------------------------------------------------------------------------
 */
int main()
{

  /*初始化天气图标数据*/
  weather_init_store();
  //updatetime = app_get_time_number();
  //maibu_get_sport_data(&last_data, 0);
  //new_win_flag = true;
  /*创建显示表盘窗口*/
  P_Window p_window = init_watch();
  if (p_window != NULL)
    {
      /*放入窗口栈显示*/

      g_window = app_window_stack_push(p_window);
    }
  /*注册一个事件通知回调，当有时间改变时，立即更新时间*/
  maibu_service_sys_event_subscribe(watch_time_change);

  /*注册接受WEB数据回调函数*/
  maibu_comm_register_web_callback(weather_web_recv_callback);
  /*注册接受手机数据回调函数*/
  maibu_comm_register_phone_callback((CBCommPhone)weather_phone_recv_callback);


  /*请求GPS数据*/
  g_comm_id_gps = maibu_comm_request_phone(ERequestPhoneGPSAltitude,  NULL, 0);
  /*注册通讯结果回调*/
  maibu_comm_register_result_callback(weather_comm_result_callback);

  return 0;
}



