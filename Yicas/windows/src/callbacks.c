#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

enum
{
  PIXBUF_COL,
  TEXT_COL
};

int largoDatos;
int histog[360];
GdkPixbuf *imagenOriginal, *imagenes[30];
int maximo[360];
int alto, ancho, rowstride, n_channels,cantMaximos;

void RGBtoHSV( float r, float g, float b, float *h, float *s, float *v )
{
	float min, max, delta;

	min =MIN( MIN( r, g), b );
	max = MAX(MAX( r, g), b );
	*v = max;				// v

	delta = max - min;

	if( max != 0 )
		*s = delta / max;		// s
	else {
		// r = g = b = 0		// s = 0, v is undefined
		*s = 0;
		*h = -1;
		return;
	}

	if( r == max )
		*h = ( g - b ) / delta;		// between yellow & magenta
	else if( g == max )
		*h = 2 + ( b - r ) / delta;	// between cyan & yellow
	else
		*h = 4 + ( r - g ) / delta;	// between magenta & cyan

	*h *= 60;				// degrees
	if( *h < 0 )
		*h += 360;

}

void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	i = h;
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
		case 0:
			*r = v;
			*g = t;
			*b = p;
			break;
		case 1:
			*r = q;
			*g = v;
			*b = p;
			break;
		case 2:
			*r = p;
			*g = v;
			*b = t;
			break;
		case 3:
			*r = p;
			*g = q;
			*b = v;
			break;
		case 4:
			*r = t;
			*g = p;
			*b = v;
			break;
		default:		// case 5:
			*r = v;
			*g = p;
			*b = q;
			break;
	}

}
static gchar *
strip_underscore (const gchar *text)
{
  gchar *p, *q;
  gchar *result;
  
  result = g_strdup (text);
  p = q = result;
  while (*p) 
    {
      if (*p != '_')
	{
	  *q = *p;
	  q++;
	}
      p++;
    }
  *q = '\0';

  return result;
}

static GtkTreeModel *
create_stock_icon_store (void)
{
  gchar *stock_id[6] = {
    GTK_STOCK_DIALOG_WARNING,
    GTK_STOCK_STOP,
    GTK_STOCK_NEW,
    GTK_STOCK_CLEAR,
    NULL,
    GTK_STOCK_OPEN    
  };

  GtkStockItem item;
  GdkPixbuf *pixbuf;
  GtkWidget *cellview;
  GtkTreeIter iter;
  GtkListStore *store;
  gchar *label;
  gint i;

  cellview = gtk_cell_view_new ();
  
  store = gtk_list_store_new (2, GDK_TYPE_PIXBUF, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS (stock_id); i++)
    {
      if (stock_id[i])
	{
	  pixbuf = gtk_widget_render_icon (cellview, stock_id[i],
					   GTK_ICON_SIZE_BUTTON, NULL);
	  gtk_stock_lookup (stock_id[i], &item);
	  label = strip_underscore (item.label);
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter,
			      PIXBUF_COL, pixbuf,
			      TEXT_COL, label,
			      -1);
	  g_object_unref (pixbuf);
	  g_free (label);
	}
      else
	{
	  gtk_list_store_append (store, &iter);
	  gtk_list_store_set (store, &iter,
			      PIXBUF_COL, NULL,
			      TEXT_COL, "separator",
			      -1);
	}
    }

  gtk_widget_destroy (cellview);
  
  return GTK_TREE_MODEL (store);
}

void
on_window1_destroy                     (GtkObject       *object,
                                        gpointer         user_data)
{
  gtk_main_quit();
}

int getHSV(guchar *p,GtkButton       *button){
  float h,s,v;
  RGBtoHSV(p[0],p[1],p[2],&h,&s,&v);
  GtkButton * hsv =(GtkButton *)lookup_widget((GtkWidget *)button,"saturacion");
  if (gtk_toggle_button_get_active(hsv))
    return s*255;
  hsv =(GtkButton *)lookup_widget((GtkWidget *)button,"matiz");
  if (gtk_toggle_button_get_active(hsv))
    return h;
  return v;
}

void normalizarHistograma(int*histog)
{
  int i,maximo=0;

  for(i=0;i<largoDatos;i++)
    if (histog[i]>maximo)
      maximo=histog[i];

  for(i=0;i<largoDatos;i++)
    histog[i]=histog[i]*100/maximo;
}

int maximoLocal(int donde,int resolucion){
  int i,ant,sig;
  ant=donde;       sig=donde;
  for (i=0;i<resolucion;i++){
    ant=(ant==0)?largoDatos-1:--ant;
    sig=(sig==largoDatos-1)?0:++sig;
    if ((histog[donde]<histog[ant])||(histog[donde]<histog[sig]))
      return 0;
  }
}

void
on_procesar_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  int i,ii;
  guchar *pixels, *p;
  int puntosDeColores=0;

  for (i=0;i<360;i++) histog[i]=0;

  GtkDrawingArea *histograma =lookup_widget((GtkWidget *)button,"histograma");
  ancho=gdk_pixbuf_get_width (imagenOriginal);
  alto=gdk_pixbuf_get_height (imagenOriginal);
  n_channels = gdk_pixbuf_get_n_channels (imagenOriginal);
  g_assert (gdk_pixbuf_get_colorspace (imagenOriginal) == GDK_COLORSPACE_RGB);
  g_assert (gdk_pixbuf_get_bits_per_sample (imagenOriginal) == 8);
  rowstride = gdk_pixbuf_get_rowstride (imagenOriginal);
  pixels = gdk_pixbuf_get_pixels (imagenOriginal);
  for (i=0;i<alto;i++)
    for (ii=0;ii<ancho;ii++){
      p = pixels + i * rowstride + ii * n_channels; //TODO: sacar como macro
      int h=getHSV(p,button);
      if (h>=0&&h<360){
        histog[h]++; //un color
        puntosDeColores++;
      }
    }
  normalizarHistograma(histog);

  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 100;
  gdk_window_invalidate_rect (((GtkWidget *)histograma)->window,
			      &update_rect,
			      FALSE);

  GtkSpinButton * spinResolucion =(GtkSpinButton *)lookup_widget((GtkWidget *)button,"resolucion");
  int resolucion=gtk_spin_button_get_value_as_int(spinResolucion);
  GtkSpinButton * spinMinimo =(GtkSpinButton *)lookup_widget((GtkWidget *)button,"minimo");
  int minimo=gtk_spin_button_get_value_as_int(spinMinimo);
  int ant,sig;
  cantMaximos=0;
  g_print ("\nMaximos en:\n",cantMaximos);
  for (i=0;i<largoDatos;i++){
    if ((histog[i]>minimo)//si supera un umbral
        &&(maximoLocal(i,resolucion) )){ //hay maximo local en i
      maximo[cantMaximos++]=i;
      g_print ("%d ",i);
    }
  }
  g_print ("\nCantidad de Maximos: %d \n",cantMaximos);
  GtkTreeModel *model=  create_stock_icon_store ();
  GtkComboBox * comboColores =(GtkComboBox *)lookup_widget((GtkWidget *)button,"comboColores");
  gtk_combo_box_set_model(comboColores,model);

  GtkButton * separar =(GtkButton *)lookup_widget((GtkWidget *)button,"separar");
  gtk_widget_set_sensitive((GtkWidget *)separar,TRUE);
}

void
on_selectorArchivo_selection_changed   (GtkFileChooser  *filechooser,
                                        gpointer         user_data)
{
  GtkSpinButton * numeroResultado =(GtkSpinButton *)lookup_widget((GtkWidget *)filechooser,"numeroResultado");
/*  int i; //TODO: liberar memoria
  for (i=0;i<gtk_spin_button_get_value_as_int(numeroResultado);i++)
    g_free(imagenes[i]);*/
  imagenOriginal = NULL;
  GError *error = NULL;
  char *filename;
  GtkImage *imagen =lookup_widget((GtkWidget *)filechooser,"imagen");
  filename = gtk_file_chooser_get_filename_utf8(filechooser);
  if (filename)
  {
    imagenOriginal = gdk_pixbuf_new_from_file_utf8 (filename, &error);			    
  }
  gtk_image_set_from_pixbuf(imagen,imagenOriginal);

  GtkButton * verResultados =(GtkButton *)lookup_widget((GtkWidget *)filechooser,"verResultados");
  GtkButton * verOriginal =(GtkButton *)lookup_widget((GtkWidget *)filechooser,"verOriginal");
  GtkButton * separar =(GtkButton *)lookup_widget((GtkWidget *)filechooser,"separar");
  GtkButton * procesar =(GtkButton *)lookup_widget((GtkWidget *)filechooser,"procesar");
  gtk_widget_set_sensitive((GtkWidget *)procesar,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)separar,FALSE);
  gtk_widget_set_sensitive((GtkWidget *)numeroResultado,FALSE);
  gtk_widget_set_sensitive((GtkWidget *)verResultados,FALSE);
  gtk_toggle_button_set_active(verOriginal,TRUE);
}

void traeRGB( int *r, int *g, int *b, int numero, GtkWidget *widget)
{
  float rf,gf,bf;
  GtkButton * hsv =(GtkButton *)lookup_widget(widget,"saturacion");
  if (gtk_toggle_button_get_active(hsv))
    HSVtoRGB(&rf,&gf,&bf,0,((float)numero)/255,1);
  hsv =(GtkButton *)lookup_widget(widget,"matiz");
  if (gtk_toggle_button_get_active(hsv))
    HSVtoRGB(&rf,&gf,&bf,numero,1,1);
  hsv =(GtkButton *)lookup_widget(widget,"valor");
  if (gtk_toggle_button_get_active(hsv))
    HSVtoRGB(&rf,&gf,&bf,0,1,((float)numero)/255);
  *r=65535*rf;*g=65535*gf;*b=65535*bf;
  
}

gboolean
on_muestra_expose_event                (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
  int r,g,b;
  int i;
  GdkColor color;
  GdkGC *gc,*copiaGc;
  gc=widget->style->fg_gc[GTK_WIDGET_STATE(widget)];
  copiaGc=gdk_gc_new(widget->window);
  for (i=0;i<largoDatos;i++){
    traeRGB( &r, &g, &b, i,widget);
    color.red=r;
    color.green=g;
    color.blue=b;
    gdk_gc_set_rgb_fg_color(gc,  &color);  
    gdk_draw_line(widget->window,gc,i,0,i,10);
  }
  gdk_gc_copy (gc,copiaGc);
  return FALSE;
}


void
on_numeroResultado_value_changed       (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
  GtkImage *imagen =(GtkImage *)lookup_widget((GtkWidget *)spinbutton,"imagen");
  GtkButton * verResultados =(GtkButton *)lookup_widget((GtkWidget *)spinbutton,"verResultados");
  gtk_image_set_from_pixbuf(imagen,imagenes[gtk_spin_button_get_value_as_int(spinbutton)-1]);
  gtk_toggle_button_set_active(verResultados,TRUE);
}


void
on_verResultados_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkImage *imagen =(GtkImage *)lookup_widget((GtkWidget *)button,"imagen");
  GtkSpinButton * numeroResultado =(GtkSpinButton *)lookup_widget((GtkWidget *)button,"numeroResultado");
  gtk_image_set_from_pixbuf(imagen,imagenes[gtk_spin_button_get_value_as_int(numeroResultado)-1]);
}


void
on_verOriginal_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkImage *imagen =(GtkImage *)lookup_widget((GtkWidget *)button,"imagen");
  gtk_image_set_from_pixbuf(imagen,imagenOriginal);
}


void
on_separar_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  int i,ii,maxActual;
  GtkSpinButton * spinUmbral =(GtkSpinButton *)lookup_widget((GtkWidget *)button,"umbral");
  int umbral=gtk_spin_button_get_value_as_int(spinUmbral);

  for (maxActual=0;maxActual<cantMaximos;maxActual++){
    imagenes[maxActual]=gdk_pixbuf_copy(imagenOriginal);

    guchar *pixels, *p, *pixelesActual;
    pixels = gdk_pixbuf_get_pixels (imagenOriginal);
    pixelesActual = gdk_pixbuf_get_pixels (imagenes[maxActual]);

    for (i=0;i<alto;i++)
      for (ii=0;ii<ancho;ii++){
        p = pixels + i * rowstride + ii * n_channels; //TODO: sacar como macro
        int diferenciaH=getHSV(p,button)-maximo[maxActual];
        p = pixelesActual + i * rowstride + ii * n_channels;
        if (abs(diferenciaH)<umbral||abs(diferenciaH)>(largoDatos-umbral)){
          p[0]=255;p[1]=255;p[2]=255;p[3]=255;
        }else{
          p[0]=0;p[1]=0;p[2]=0;p[3]=0;
        }
      }
  }
  if (cantMaximos>0){
    GtkSpinButton * numeroResultado =(GtkSpinButton *)lookup_widget((GtkWidget *)button,"numeroResultado");
    GtkButton * verResultados =(GtkButton *)lookup_widget((GtkWidget *)button,"verResultados");
    gtk_spin_button_set_value(numeroResultado,cantMaximos) ;
    gtk_spin_button_set_range(numeroResultado,1,cantMaximos);
    gtk_spin_button_set_value(numeroResultado,cantMaximos) ;
    gtk_widget_set_sensitive((GtkWidget *)numeroResultado,TRUE);
    gtk_widget_set_sensitive((GtkWidget *)verResultados,TRUE);
    gtk_toggle_button_set_active(verResultados,TRUE);
  }
}




void
on_saturacion_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  largoDatos=256;
  GtkWidget *muestra =lookup_widget(button,"muestra");
  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 11;
  gdk_window_invalidate_rect (muestra->window,
			      &update_rect,
			      FALSE);
}


void
on_valor_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{
  largoDatos=256;
  GtkWidget *muestra =lookup_widget(button,"muestra");
  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 11;
  gdk_window_invalidate_rect (muestra->window,
			      &update_rect,
			      FALSE);
}


void
on_matiz_clicked                       (GtkButton       *button,
                                        gpointer         user_data)
{
  largoDatos=360;
  GtkWidget *muestra =lookup_widget(button,"muestra");
  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 11;
  gdk_window_invalidate_rect (muestra->window,
			      &update_rect,
			      FALSE);
}


gboolean
on_histograma_expose_event             (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data)
{
  int i;
  for (i=0;i<largoDatos;i++)
    gdk_draw_line(widget->window,widget->style->fg_gc[GTK_WIDGET_STATE (widget)],i,100,i,100-histog[i]);
  return FALSE;
}


void
on_window1_show                        (GtkWidget       *widget,
                                        gpointer         user_data)
{
g_print ("show \n"); 
  GtkWidget *histograma =lookup_widget(widget,"histograma");
  GtkWidget *muestra =lookup_widget(widget,"muestra");
  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 11;
  gdk_window_invalidate_rect (muestra->window,
			      &update_rect,
			      FALSE);
  update_rect.height = 100;
  gdk_window_invalidate_rect (histograma->window,
			      &update_rect,
			      FALSE);
}


void
on_window1_realize                     (GtkWidget       *widget,
                                        gpointer         user_data)
{
g_print ("realize \n"); 
  imagenOriginal = NULL;
  int i;
  largoDatos=360;
  for (i=0;i<360;i++) histog[i]=0;
}


void
on_valor_value_changed                 (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{

}


void
on_borrarMaximo_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{

}


void
on_valorMaximo_value_changed           (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{

}

