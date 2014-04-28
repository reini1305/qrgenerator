#include <pebble.h>

static Window *window;
static Layer *qr_layer;
static Tuple *qr_tuple;
static TextLayer *description_layer;
static char description_text[41];
static AppTimer *light_timer;
static AppTimer *descr_timer;

enum {
  QUOTE_KEY_QRCODE = 0x0,
  QUOTE_KEY_FETCH = 0x1,
  QUOTE_KEY_DESCRIPTION = 0x2
};

static bool send_to_phone_multi(int quote_key, char *symbol) {
  if ((quote_key == -1) && (symbol == NULL)) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "no data to send");
    // well, the "nothing" that was sent to us was queued, anyway ...
    return true;
  }
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "null iter");
    return false;
  }

  Tuplet tuple = (symbol == NULL)
                      ? TupletInteger(quote_key, 1)
                      : TupletCString(quote_key, symbol);
  dict_write_tuplet(iter, &tuple);
  dict_write_end(iter);

  app_message_outbox_send();
  return true;
}

static void hide_layer(void *data)
{
  layer_set_hidden(text_layer_get_layer(description_layer), true);
}

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

static void show_description(void)
{
  layer_set_hidden(text_layer_get_layer(description_layer), false);
  if(!app_timer_reschedule(descr_timer,1500))
    descr_timer = app_timer_register(1500,hide_layer,NULL);
}

static void description_click_handler(ClickRecognizerRef recognizer, void *context) {
  show_description();
  enable_light();
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  qr_tuple = dict_find(iter, QUOTE_KEY_QRCODE);
  if (qr_tuple) {
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Length of String: %d",qr_tuple->length);
    layer_mark_dirty(qr_layer);
    layer_set_hidden(text_layer_get_layer(description_layer), true);
  }
  Tuple *descr_tuple = dict_find(iter, QUOTE_KEY_DESCRIPTION);
  if(descr_tuple)
  {
    strncpy(description_text, descr_tuple->value->cstring, 40);
    text_layer_set_text(description_layer, description_text);
    show_description();

  }
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
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Code Length: %d",code_length);
    GRect bounds = layer_get_bounds(layer);
    int pixel_size = bounds.size.w/code_length;
    int width = pixel_size*code_length;
    int offset_x = (bounds.size.w-width)/2;
    APP_LOG(APP_LOG_LEVEL_DEBUG,"Pixel Size: %d",pixel_size);
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
  window_single_repeating_click_subscribe(BUTTON_ID_UP, repeat_interval_ms, (ClickHandler) description_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, repeat_interval_ms, (ClickHandler) description_click_handler);
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
  
  description_layer = text_layer_create(GRect(offset, bounds.size.h/2, bounds.size.w, bounds.size.h));
  text_layer_set_text_alignment(description_layer,GTextAlignmentCenter);
  text_layer_set_font(description_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text(description_layer, "Loading...");
  layer_add_child(window_layer, text_layer_get_layer(description_layer));
  
  enable_light();
  
  send_to_phone_multi(QUOTE_KEY_FETCH,NULL);
}

static void window_unload(Window *window) {
  layer_destroy(qr_layer);
  layer_destroy(text_layer_get_layer(description_layer));
  light_enable(false);
}

static void init(void) {

  app_message_init();


  window = window_create();

  //window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
