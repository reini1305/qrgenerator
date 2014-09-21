#include <pebble.h>

#define TEXT_LENGTH 128

static Window *window;
static Layer *qr_layer;
static Tuple *qr_tuple;
static TextLayer *description_layer;
static char description_text[TEXT_LENGTH];
static AppTimer *light_timer;
static PropertyAnimation *animation=NULL;
static bool animation_finished=true;
//static AppTimer *descr_timer;
static int id=4;

enum {
  QUOTE_KEY_QRCODE = 0x0,
  QUOTE_KEY_FETCH = 0x1,
  QUOTE_KEY_DESCRIPTION = 0x2,
  QUOTE_KEY_ID=0x3
};

static void animation_stopped(PropertyAnimation *animation, bool finished, void *data) {
  property_animation_destroy(animation);
  animation_finished=true;
  layer_set_frame(text_layer_get_layer(description_layer),GRect(0, 144, 5*144, 24));
}

static void animate_layer_bounds(Layer* layer, GRect toRect)
{
  if(!animation_finished)
  {
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"in cleanup...");
    animation_unschedule((Animation*)animation);
    //property_animation_destroy(animation);
    animation_finished=true;
    layer_set_frame(text_layer_get_layer(description_layer),GRect(0, 144, 5*144, 24));
  }
  animation = property_animation_create_layer_frame(layer, NULL, &toRect);
  animation_set_handlers((Animation*) animation, (AnimationHandlers) {
    .stopped = (AnimationStoppedHandler) animation_stopped,
  }, NULL);
  animation_set_duration((Animation*)animation,3000);
  animation_set_curve((Animation*)animation,AnimationCurveEaseInOut);
  animation_finished=false;
  //APP_LOG(APP_LOG_LEVEL_DEBUG,"animation running...");
  animation_schedule((Animation*)animation);
}

void show_description(char* new_text)
{
  strncpy(description_text, new_text, TEXT_LENGTH-1);
  //text_layer_set_text(description_layer,new_text);
  // Scroll to the right
  GRect current_size = layer_get_frame(text_layer_get_layer(description_layer));
  GSize new_size = text_layer_get_content_size(description_layer);
  animate_layer_bounds(text_layer_get_layer(description_layer), GRect(current_size.origin.x-(new_size.w<144?144:new_size.w)+144,current_size.origin.y,
                                                                      5*144,current_size.size.h));
}

static bool send_to_phone_multi(int quote_key, int symbol) {
  
  // Loadinating
  show_description("Loading...");
  //layer_set_hidden(text_layer_get_layer(description_layer), false);
  
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  Tuplet tuple = TupletInteger(quote_key, symbol);
  dict_write_tuplet(iter, &tuple);
  dict_write_end(iter);

  app_message_outbox_send();
  return true;
}

/*static void hide_layer(void *data)
{
  layer_set_hidden(text_layer_get_layer(description_layer), true);
}*/

static void light_off(void *data)
{
  light_enable(false);
}

static void enable_light(void)
{
  light_enable(true);
  if(!app_timer_reschedule(light_timer,10000))
    light_timer = app_timer_register(10000,light_off,NULL);
}

static void description_click_handler(ClickRecognizerRef recognizer, void *context) {
  show_description(description_text);
  enable_light();
}

static void previous_click_handler(ClickRecognizerRef recognizer, void *context) {
  id--;
  if(id<0)
    id=5;
  send_to_phone_multi(QUOTE_KEY_FETCH,id+1);
}
static void next_click_handler(ClickRecognizerRef recognizer, void *context) {
  id++;
  if(id>5)
    id=0;
  send_to_phone_multi(QUOTE_KEY_FETCH,id+1);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  qr_tuple = dict_find(iter, QUOTE_KEY_QRCODE);
  if (qr_tuple) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Length of String: %d",qr_tuple->length);
    layer_mark_dirty(qr_layer);
    //layer_set_hidden(text_layer_get_layer(description_layer), true);
  }
  Tuple *descr_tuple = dict_find(iter, QUOTE_KEY_DESCRIPTION);
  if(descr_tuple)
  {
    show_description(descr_tuple->value->cstring);
  }
  /*Tuple *id_tuple = dict_find(iter,QUOTE_KEY_ID);
  if(id_tuple)
  {
    id = id_tuple->value->int32+1;
  }*/
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
}


static void app_message_init(void) {
  // Register message handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  // Init buffers
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static int my_sqrt(int value)
{
  int answer = value;
  int answer_sqr = value*value;
  while(answer_sqr>value)
  {
    answer = (answer + value/answer)/2;
    answer_sqr = answer * answer;
  }
  return answer;
}

static void qr_layer_draw(Layer *layer, GContext *ctx)
{
  if(qr_tuple){
    enable_light();
    int code_length = my_sqrt(qr_tuple->length);
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Code Length: %d",code_length);
    GRect bounds = layer_get_bounds(layer);
    int pixel_size = bounds.size.w/code_length;
    int width = pixel_size*code_length;
    int offset_x = (bounds.size.w-width)/2;
    //APP_LOG(APP_LOG_LEVEL_DEBUG,"Pixel Size: %d",pixel_size);
    GRect current_pixel = GRect(0, 0, pixel_size,pixel_size);
    graphics_context_set_fill_color(ctx, GColorBlack);
    for (int i=0;i<code_length;i++)
    {
      for (int j=0;j<code_length;j++)
      {
        if(qr_tuple->value->cstring[i*code_length+j]==1)
        {
          current_pixel.origin.x = j*pixel_size+offset_x;
          current_pixel.origin.y = i*pixel_size+offset_x;
          graphics_fill_rect(ctx, current_pixel,0,GCornerNone);
        }
      }
    }
  }
}

static void window_click_config_provider(void *context) {
  const uint16_t repeat_interval_ms = 1000;
  window_single_repeating_click_subscribe(BUTTON_ID_UP, repeat_interval_ms, (ClickHandler) previous_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, repeat_interval_ms, (ClickHandler) next_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_SELECT, repeat_interval_ms, (ClickHandler) description_click_handler);
}

static void window_load(Window *window) {
  window_set_click_config_provider(window,window_click_config_provider);
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  int offset = 4;
  bounds.origin.x = offset;
  bounds.origin.y = offset;
  bounds.size.h -= offset*2;
  bounds.size.w -= offset*2;
  qr_layer = layer_create(bounds);
  layer_add_child(window_layer, qr_layer);
  layer_set_update_proc(qr_layer, qr_layer_draw);
  
  layer_set_clips(window_layer,false);
  description_layer = text_layer_create(GRect(0, 144, 5*144, 24));
  layer_set_clips(text_layer_get_layer(description_layer),false);
  //text_layer_set_text_alignment(description_layer,GTextAlignmentCenter);
  text_layer_set_font(description_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(description_layer,GColorBlack);
  text_layer_set_text_color(description_layer,GColorWhite);
  text_layer_set_text(description_layer,description_text);
  show_description("Loading...");
  layer_add_child(window_layer, text_layer_get_layer(description_layer));
  
  enable_light();
  
  send_to_phone_multi(QUOTE_KEY_FETCH,id+1);
}

static void window_unload(Window *window) {
  layer_destroy(qr_layer);
  layer_destroy(text_layer_get_layer(description_layer));
  light_enable(false);
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  // Process tap on ACCEL_AXIS_X, ACCEL_AXIS_Y or ACCEL_AXIS_Z
  // Direction is 1 or -1
  enable_light();
}


static void init(void) {

  app_message_init();
  
  animation_finished=true;
  
  accel_tap_service_subscribe(&accel_tap_handler);

  window = window_create();

  //window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_set_fullscreen(window,true);
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
  accel_tap_service_unsubscribe();
}

int main(void) {
  init();

  //APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
