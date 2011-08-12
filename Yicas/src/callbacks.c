#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

enum
{// campos del arreglo de maximos
  ANCHO, VALOR, ACTIVO, NOBJETOS
};

enum
{// campos del arreglo de segmentos
  LARGO,ANGULO
};

enum
{//direcciones
  IMPOSIBLE, N, NE, E, SE, S, SO, O, NO
};
double segmento[2][1000][2];
int cantidadSegmentos[2];
int largoDatos;
int histog[360];
int maximo[360][4];
int objeto[360][2000][2];
GdkPixbuf *imagenOriginal, *imagenes[30];
int alto, ancho, rowstride, n_channels,cantMaximos;
GtkButton *segmentar, *separar, *procesar, *estudiarFormas;
GtkToggleButton *verResultados, *verOriginal, *saturacion, *matiz, *valor, *forma1, *forma2;
GtkSpinButton *umbral, *valorMaximo, *numeroResultado, *spinResolucion, *spinMinimo, *numObjeto, *largoSeg, *tolAngulo, *toleranciaComparaAngulo, *toleranciaCompara;
GtkCheckButton *activo;
GtkComboBox *comboColores;
GtkDrawingArea *histograma;
GtkWidget *muestra;
GtkImage *imagen;
GtkLabel *igualODiferente;
    
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

void traeRGB( int *r, int *g, int *b, int numero, GtkWidget *widget)
{
  float rf,gf,bf;

  if (gtk_toggle_button_get_active(saturacion))
    HSVtoRGB(&rf,&gf,&bf,0,((float)numero)/255,1);
  if (gtk_toggle_button_get_active(matiz))
    HSVtoRGB(&rf,&gf,&bf,numero,1,1);
  if (gtk_toggle_button_get_active(valor))
    HSVtoRGB(&rf,&gf,&bf,0,1,((float)numero)/255);
  *r=65535*rf;*g=65535*gf;*b=65535*bf;
}

GtkTreeModel *
create_stock_icon_store (GtkWidget * unWidget)
{
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;
  GtkListStore *store;
  int i;

  store = gtk_list_store_new (2,G_TYPE_INT, GDK_TYPE_PIXBUF);

  for (i = 0; i <cantMaximos; i++)
    {
        pixbuf = gdk_pixbuf_new                      (GDK_COLORSPACE_RGB,
                                                         FALSE,
                                                         8,
                                                          GTK_ICON_SIZE_BUTTON,
                                                          GTK_ICON_SIZE_BUTTON);
        int rowstride = gdk_pixbuf_get_rowstride (pixbuf);
        guchar *pixels, *p;
        pixels = gdk_pixbuf_get_pixels (pixbuf);
        int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
        int x,y;
        int r,g,b;
        traeRGB(&r,&g,&b,maximo[i][VALOR],unWidget);
        r=r/65535.0*255;g=g/65535.0*255;
        b=b/65535.0*255;
        g_print ("\nR:%d G:%d B:%d ",r,g,b);
        for(x=0;x<GTK_ICON_SIZE_BUTTON;x++)
             for(y=0;y<	GTK_ICON_SIZE_BUTTON;y++){
               p = pixels + x * rowstride + y * n_channels;
               p[0]=r;p[1]=g;p[2]=b;p[3]=255;
        }
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter,
	                      0, i+1,
			      1, pixbuf,
			      -1);
        g_object_unref (pixbuf);

    }
  return GTK_TREE_MODEL (store);
}

int getHSV(guchar *p,GtkButton       *button){
  float h,s,v;
  RGBtoHSV(p[0],p[1],p[2],&h,&s,&v);
  if (gtk_toggle_button_get_active(saturacion))
    return s*255;
  if (gtk_toggle_button_get_active(matiz))
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
  ant=donde;
  sig=donde;
  for (i=0;i<resolucion;i++){
    ant=(ant==0)?largoDatos-1:--ant;
    sig=(sig==largoDatos-1)?0:++sig;
    if ((histog[donde]<histog[ant])||(histog[donde]<histog[sig]))
      return 0;
  }
  return 1;    
}

void
on_procesar_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  int i,ii;
  guchar *pixels, *p;
  int puntosDeColores=0;

  for (i=0;i<360;i++) histog[i]=0;
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


  int resolucion=gtk_spin_button_get_value_as_int(spinResolucion);
  int minimo=gtk_spin_button_get_value_as_int(spinMinimo);
  int ant,sig;
  cantMaximos=0;
  g_print ("\nMaximos en:\n");
  for (i=0;i<largoDatos;i++){
    if ((histog[i]>minimo)//si supera un umbral
        &&(maximoLocal(i,resolucion) )){ //hay maximo local en i
      maximo[cantMaximos][ANCHO]=20;
      maximo[cantMaximos][ACTIVO]=1;
      maximo[cantMaximos][VALOR]=i;
      maximo[cantMaximos++][NOBJETOS]=0;
      g_print (" %d ",i);
      i+=resolucion;      
    }
  }
  g_print ("\nCantidad de Maximos: %d \n",cantMaximos);
  GtkTreeModel *model=  create_stock_icon_store ((GtkWidget *)button);
  gtk_combo_box_set_model(comboColores,model);
  gtk_widget_set_sensitive((GtkWidget *)umbral,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)valorMaximo,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)activo,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)comboColores,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)separar,TRUE);
  gtk_combo_box_set_active(comboColores,0);
}

void
on_selectorArchivo_selection_changed   (GtkFileChooser  *filechooser,
                                        gpointer         user_data)
{
  int i; 
  for (i=0;i<cantMaximos;i++)
    if (imagenes[i]) g_object_unref ((imagenes[i]));
  if (imagenOriginal) g_object_unref (imagenOriginal);
  GError *error = NULL;
  char *filename;

  filename = gtk_file_chooser_get_filename(filechooser);
  if (filename)
  {
    imagenOriginal = NULL;
    cantMaximos=0;
    imagenOriginal = gdk_pixbuf_new_from_file(filename, &error);

    gtk_image_set_from_pixbuf(imagen,imagenOriginal);


    gtk_widget_set_sensitive((GtkWidget *)estudiarFormas,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)procesar,TRUE);
    gtk_widget_set_sensitive((GtkWidget *)separar,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)numeroResultado,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)verResultados,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)umbral,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)valorMaximo,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)activo,FALSE);
    gtk_widget_set_sensitive((GtkWidget *)comboColores,FALSE);
    gtk_toggle_button_set_active(verOriginal,TRUE);
    gtk_widget_set_sensitive((GtkWidget *)segmentar,FALSE);    
    gtk_widget_set_sensitive((GtkWidget *)numObjeto,FALSE);    
  }
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
  gtk_image_set_from_pixbuf(imagen,imagenes[gtk_spin_button_get_value_as_int(spinbutton)-1]);
  gtk_toggle_button_set_active(verResultados,TRUE);
  gtk_spin_button_set_range(numObjeto,1,maximo[gtk_spin_button_get_value_as_int(numeroResultado)-1][NOBJETOS]);
  gtk_spin_button_set_value(numObjeto,maximo[gtk_spin_button_get_value_as_int(numeroResultado)-1][NOBJETOS]) ;

}


void
on_verResultados_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_image_set_from_pixbuf(imagen,imagenes[gtk_spin_button_get_value_as_int(numeroResultado)-1]);
}


void
on_verOriginal_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_image_set_from_pixbuf(imagen,imagenOriginal);
}


void
on_separar_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  int i,ii,maxActual;
  for (i=0;i<cantMaximos;i++)
    if (imagenes[i]) g_object_unref ((imagenes[i]));  
  for (maxActual=0;maxActual<cantMaximos;maxActual++){
    imagenes[maxActual]=gdk_pixbuf_copy(imagenOriginal);
    guchar *pixels, *p, *pixelesActual;
    pixels = gdk_pixbuf_get_pixels (imagenOriginal);
    pixelesActual = gdk_pixbuf_get_pixels (imagenes[maxActual]);
    if (maximo[maxActual][ACTIVO]){
      for (i=0;i<alto;i++)
        for (ii=0;ii<ancho;ii++){
          p = pixels + i * rowstride + ii * n_channels; //TODO: sacar como macro
          int diferenciaH=getHSV(p,button)-maximo[maxActual][VALOR];
          p = pixelesActual + i * rowstride + ii * n_channels;
          if (abs(diferenciaH)<maximo[maxActual][ANCHO]||abs(diferenciaH)>(largoDatos-maximo[maxActual][ANCHO])){
            p[0]=255;p[1]=255;p[2]=255;p[3]=255;
          }else{
            p[0]=0;p[1]=0;p[2]=0;p[3]=0;
          }
        }
    }
  }
  if (cantMaximos>0){
    gtk_spin_button_set_value(numeroResultado,cantMaximos) ;
    gtk_spin_button_set_range(numeroResultado,1,cantMaximos);
    gtk_spin_button_set_value(numeroResultado,cantMaximos) ;
    gtk_widget_set_sensitive((GtkWidget *)segmentar,TRUE);
    gtk_widget_set_sensitive((GtkWidget *)numeroResultado,TRUE);
    gtk_widget_set_sensitive((GtkWidget *)verResultados,TRUE);
    gtk_toggle_button_set_active((GtkToggleButton *)verResultados,TRUE);
  }
}




void
on_saturacion_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  largoDatos=256;
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
on_valorMaximo_value_changed           (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
  int max=gtk_combo_box_get_active(comboColores);
  maximo[max][VALOR]=gtk_spin_button_get_value_as_int(spinbutton);
}


void
on_umbral_value_changed                (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
  int max=gtk_combo_box_get_active(comboColores);
  maximo[max][ANCHO]=gtk_spin_button_get_value_as_int(spinbutton);
}


void
on_comboColores_changed                (GtkComboBox     *combobox,
                                        gpointer         user_data)
{
  int max=gtk_combo_box_get_active(combobox);
  gtk_spin_button_set_value(umbral,maximo[max][ANCHO]);
  gtk_spin_button_set_value(valorMaximo,maximo[max][VALOR]);
  gtk_toggle_button_set_active((GtkToggleButton*)activo,(gboolean)maximo[max][ACTIVO]);
}



void
on_activo_clicked                      (GtkButton       *button,
                                        gpointer         user_data)
{
  int max=gtk_combo_box_get_active(comboColores);
  maximo[max][ACTIVO]=gtk_toggle_button_get_active((GtkToggleButton*)button);
}

int estaPintado( int x, int y, guchar *pixels){
    guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro
    if (p[0]>0)
      return TRUE;
    else
      return FALSE;

}
int yaPaso( int x, int y, guchar *pixels){
    guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro
    if (p[1]==150){
      g_print ("\nfin del objeto");
      return TRUE;
    }else
      return FALSE;

}

int yaSegmento( int x, int y, guchar *pixels){
    guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro
    if (p[0]==255 && p[1]==255 && p[2]==0){
      return TRUE;
    }else
      return FALSE;
}

int yaSegmentoAlrededor( int x, int y, guchar *pixels){

  if (y<alto && x<ancho &&  yaSegmento(x,y+1,pixels) )
      return TRUE;
  if (y<alto && x<ancho &&  yaSegmento(x+1,y+1,pixels))
      return TRUE;
  if (y>0 && x<ancho &&  yaSegmento(x+1,y,pixels) )
      return TRUE;
  if (y>0 && x<ancho &&  yaSegmento(x+1,y-1,pixels))
      return TRUE;
  if (y>0 && x>0 &&  yaSegmento(x,y-1,pixels) )
      return TRUE;
  if (y>0 && x>0 &&  yaSegmento(x-1,y-1,pixels))
      return TRUE;
  if (y<alto && x>0 &&  yaSegmento(x-1,y,pixels))
      return TRUE;
  if (y<alto && x>0 &&  yaSegmento(x-1,y+1,pixels))
      return TRUE;
return FALSE;
}

void buscarProximoObjeto( int *x, int *y,int objActual,int actual){

     if (objActual<maximo[actual][NOBJETOS]){
       *x=objeto[actual][objActual][0]-1;
       *y=objeto[actual][objActual][1];
       g_print ("\nobjeto %d en: %d %d",objActual,*x,*y);

     }

}

int cuentaLibres(int x, int y, guchar *pixels){
  guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro
  int a,b,nx,ny,resultado=0;
  for (a=-1;a<2;a++)
    for (b=-1;b<2;b++){
      if (a!=0 || b!=0){
        nx=x+a; ny=y+b;
        if (y<alto && x<ancho && y<alto && x>0 )
        if (!estaPintado(nx,ny,pixels)) resultado++;
      }
  }
  return resultado;
}


int sigDireccion( int x, int y, guchar *pixels){
  guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro

  if (y<alto && x<ancho && estaPintado(x,y+1,pixels)  && !estaPintado(x+1,y+1,pixels) && !yaPaso(x+1,y+1,pixels) && cuentaLibres(x+1,y+1,pixels)>1)
      return SE;
  if (y<alto && x<ancho && estaPintado(x+1,y+1,pixels)  && !estaPintado(x+1,y,pixels) && !yaPaso(x+1,y,pixels) && cuentaLibres(x+1,y,pixels)>1)
      return E;
  if (y>0 && x<ancho && estaPintado(x+1,y,pixels)  && !estaPintado(x+1,y-1,pixels) && !yaPaso(x+1,y-1,pixels) && cuentaLibres(x+1,y-1,pixels)>1)
      return NE;
  if (y>0 && x<ancho && estaPintado(x+1,y-1,pixels)  && !estaPintado(x,y-1,pixels) && !yaPaso(x,y-1,pixels) && cuentaLibres(x,y-1,pixels)>1)
      return N;
  if (y>0 && x>0 && estaPintado(x,y-1,pixels)  && !estaPintado(x-1,y-1,pixels) && !yaPaso(x-1,y-1,pixels) && cuentaLibres(x-1,y-1,pixels)>1)
      return NO;
  if (y>0 && x>0 && estaPintado(x-1,y-1,pixels)  && !estaPintado(x-1,y,pixels) && !yaPaso(x-1,y,pixels) && cuentaLibres(x-1,y,pixels)>1)
      return O;
  if (y<alto && x>0 && estaPintado(x-1,y,pixels)  && !estaPintado(x-1,y+1,pixels) && !yaPaso(x-1,y+1,pixels) && cuentaLibres(x-1,y+1,pixels)>1)
      return SO;
  if (y<alto && x>0 && estaPintado(x-1,y+1,pixels)  && !estaPintado(x,y+1,pixels) && !yaPaso(x,y+1,pixels) && cuentaLibres(x,y+1,pixels)>1)
      return S;
return IMPOSIBLE;
}
void imprimeDireccion(int direccion){
 
    switch (direccion){
      case   N:
      g_print (" N");
      break;
      case   NE:
      g_print (" NE");
      break;
      case   E:
      g_print (" E");
      break;
      case   SE:
      g_print (" SE");
      break;
      case   S:
      g_print (" S");
      break;
      case   SO:
      g_print (" SO");
      break;
      case   O:
      g_print (" O");
      break;
      case   NO:
      g_print (" NO");
      break;
      default:
      break;
      }
}      
      
void mover(int direccion, int *x, int *y,guchar *pixels){
    guchar *p = pixels + *y * rowstride + *x * n_channels; //TODO: sacar como macro

    p[1]=150;
    imprimeDireccion(direccion);
    switch (direccion){
      case   N:
      --(*y);
      break;
      case   NE:
      --(*y); ++(*x);
      break;
      case   E:
      ++(*x);
      break;
      case   SE:
      ++(*y); ++(*x);
      break;
      case   S:
      ++(*y);
      break;
      case   SO:
      ++(*y); --(*x);
      break;
      case   O:
      --(*x);
      break;
      case   NO:
      --(*y); --(*x);
      break;
      default:
      break;
      }

 }


pintarRecursivo(int x,int y,int canal,int color,guchar *pixels,int vida,int r,int g,int b){
  guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro
  if (p[0]==r&&p[1]==g&&p[2]==b){
   p[canal]=color;
   if (vida>0)
     vida--;
   if (vida!=0){
    if (x>0 )
      pintarRecursivo(x-1,y,canal,color,pixels,vida,r,g,b);
    if (y>0)
      pintarRecursivo(x,y-1,canal,color,pixels,vida,r,g,b);
    if (x<ancho)
      pintarRecursivo(x+1,y,canal,color,pixels,vida,r,g,b);
    if (y<alto)
      pintarRecursivo(x,y+1,canal,color,pixels,vida,r,g,b);
   }
  }
}


void segmentarCapa(guchar *pixels,int actual){
     int x,y;
     maximo[actual][NOBJETOS]=0;
     for (y=0;y<alto;y++){
       for (x=0;x<ancho;x++){
          guchar *p = pixels + y * rowstride + x * n_channels; //TODO: sacar como macro

          if (p[0]==255&&p[1]==255&&p[2]==255){

            if (!yaSegmentoAlrededor(x,y,pixels)){
              objeto[actual][maximo[actual][NOBJETOS]][0]=x; objeto[actual][maximo[actual][NOBJETOS]][1]=y;
              maximo[actual][NOBJETOS]++;
            }
            pintarRecursivo(x,y,2,0,pixels,500,255,255,255);

          }
       }
     }
     g_print ("\ncapa: %d. cantidad de objetos: %d",actual,maximo[actual][NOBJETOS]);
}

void procesarSegmentito(int x,int y,int*xAnalisis,int*yAnalisis,int*xinic,int*yinic,int* seg){
         int formaActual= gtk_toggle_button_get_active(forma1)?0:1;
         int toleranciaAngulo=gtk_spin_button_get_value_as_int(tolAngulo)-1;
         double yAng=y-*yAnalisis;
         double xAng=x-*xAnalisis;
//         double yAng=y-*yinic;
  //       double xAng=x-*xinic;
         double anguloActual=180*atan2(yAng,xAng)/3.1415;
         if (anguloActual<0) anguloActual+=360;
         g_print ("\ny %d yAnalisis %d x %d xAnalisis %d, angulo: %f, diferencia : %f",y,*yAnalisis,x,*xAnalisis,anguloActual,anguloActual-segmento[formaActual][*seg][ANGULO]);


         if ((*seg==-1)||((abs((int)anguloActual-(int)segmento[formaActual][*seg][ANGULO])>toleranciaAngulo)&&(abs((int)anguloActual-(int)segmento[formaActual][*seg][ANGULO])<360-toleranciaAngulo))){
             //cambio de segmento


         yAng=y-*yAnalisis;
         xAng=x-*xAnalisis;
           *seg=*seg+1;
           if (*seg>0){
             *xinic=x;
             *yinic=y;
           }
         }else{
           yAng=y-*yinic;
           xAng=x-*xinic;
         }
         anguloActual=180*atan2(yAng,xAng)/3.1415;
         if (anguloActual<0) anguloActual+=360;
         segmento[formaActual][*seg][LARGO]=sqrt(yAng*yAng+xAng*xAng);
         segmento[formaActual][*seg][ANGULO]=anguloActual;
         *xAnalisis=x;
         *yAnalisis=y;

}


void   
on_estudiarFormas_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  int formaActual= gtk_toggle_button_get_active(forma1)?0:1;
  int actual=gtk_spin_button_get_value_as_int(numeroResultado)-1;
  int nobjeto=gtk_spin_button_get_value_as_int(numObjeto)-1;
  int largoAnalisis=gtk_spin_button_get_value_as_int(largoSeg)-1;
  if (maximo[actual][ACTIVO]){
    GdkPixbuf *pixbuf=imagenes[actual];
    guchar *pixels, *p;
    pixels = gdk_pixbuf_get_pixels (pixbuf);
    int nuevaDirec,xAnalisis,yAnalisis,x,y,xinic,yinic;
    xAnalisis=yAnalisis=x=y=xinic=yinic=0;
    buscarProximoObjeto(&x,&y,nobjeto,actual);
    xinic=xAnalisis=x;
    yinic=yAnalisis=y;
    int pc=0;
    int seg=-1;
    int t=0;
    while(TRUE){
      t++;
      if (((t>20) && (abs(x-objeto[actual][nobjeto][0]-1)<5)&&(abs(y-objeto[actual][nobjeto][1])<5) )||t>1500)  {
        procesarSegmentito(x,y,&xAnalisis,&yAnalisis,&xinic,&yinic,&seg);
        cantidadSegmentos[formaActual]=seg;
        for (seg=0;seg<cantidadSegmentos[formaActual];seg++)
          g_print ("\nSegmento: %d. Angulo: %f. Longitud: %f",seg,segmento[formaActual][seg][ANGULO],segmento[formaActual][seg][LARGO]);
        break;
      }
      if (pc==largoAnalisis){
         procesarSegmentito(x,y,&xAnalisis,&yAnalisis,&xinic,&yinic,&seg);
         pc=0;
      }
      pc++;
      nuevaDirec=sigDireccion(x,y,pixels);
      mover(nuevaDirec,&x,&y,pixels);
    }
  }
  gdk_window_invalidate_region (gtk_widget_get_parent_window((GtkWidget *)imagen)  ,
			       gdk_drawable_get_visible_region ((GdkDrawable*)gtk_widget_get_parent_window((GtkWidget *)imagen) ),
			      FALSE);  
}

void
on_segmentar_clicked                   (GtkButton       *button,
                                        gpointer         user_data)
{
  int actual;
  for (actual=0;actual<cantMaximos;actual++){
   if (maximo[actual][ACTIVO]){
    GdkPixbuf *pixbuf=imagenes[actual];
    guchar *pixels, *p;
    pixels = gdk_pixbuf_get_pixels (pixbuf);
    segmentarCapa(pixels,actual);
   }
  }
  gtk_spin_button_set_range(numObjeto,1,maximo[gtk_spin_button_get_value_as_int(numeroResultado)-1][NOBJETOS]);
  gtk_spin_button_set_value(numObjeto,maximo[gtk_spin_button_get_value_as_int(numeroResultado)-1][NOBJETOS]) ;
  gdk_window_invalidate_region (gtk_widget_get_parent_window((GtkWidget *)imagen)  ,
			       gdk_drawable_get_visible_region ((GdkDrawable*)gtk_widget_get_parent_window((GtkWidget *)imagen) ),
			      FALSE);
  
  gtk_widget_set_sensitive((GtkWidget *)estudiarFormas,TRUE);
  gtk_widget_set_sensitive((GtkWidget *)numObjeto,TRUE);

}

void
on_window1_destroy                     (GtkObject       *object,
                                        gpointer         user_data)
{
  gtk_main_quit();
}

void
on_window1_show                        (GtkWidget       *widget,
                                        gpointer         user_data)
{
  GdkRectangle update_rect;
  update_rect.x = 0;
  update_rect.y = 0;
  update_rect.width = 360;
  update_rect.height = 11;
  gdk_window_invalidate_rect (muestra->window,
			      &update_rect,
			      FALSE);
  update_rect.height = 100;
  gdk_window_invalidate_rect (((GtkWidget*)histograma)->window,
			      &update_rect,
			      FALSE);
}

void
on_window1_realize                     (GtkWidget       *widget,
                                        gpointer         user_data)
{
  imagenOriginal = NULL;
  cantMaximos=0;
  largoDatos=360;
  int i;
  for (i=0;i<360;i++) histog[i]=0;
  for (i=0;i<30;i++) imagenes[i]=NULL;
  
  verResultados =(GtkToggleButton *)lookup_widget(widget,"verResultados");
  verOriginal =(GtkToggleButton  *)lookup_widget(widget,"verOriginal");
  separar =(GtkButton *)lookup_widget(widget,"separar");
  procesar =(GtkButton *)lookup_widget(widget,"procesar");
  estudiarFormas =(GtkButton *)lookup_widget(widget,"estudiarFormas");
  umbral =(GtkSpinButton *)lookup_widget(widget,"umbral");
  valorMaximo =(GtkSpinButton *)lookup_widget(widget,"valorMaximo");
  activo =(GtkCheckButton *)lookup_widget(widget,"activo");
  comboColores=(GtkComboBox *)lookup_widget(widget,"comboColores");
  numeroResultado =(GtkSpinButton *)lookup_widget(widget,"numeroResultado");
  segmentar =(GtkButton *)lookup_widget(widget,"segmentar");  
  histograma =(GtkDrawingArea *)lookup_widget(widget,"histograma");
  spinResolucion =(GtkSpinButton *)lookup_widget(widget,"resolucion");
  saturacion=(GtkToggleButton *)lookup_widget(widget,"saturacion");
  matiz=(GtkToggleButton *)lookup_widget(widget,"matiz");
  valor=(GtkToggleButton *)lookup_widget(widget,"valor");
  spinMinimo =(GtkSpinButton *)lookup_widget(widget,"minimo");
  muestra =lookup_widget(widget,"muestra");
  imagen =(GtkImage *)lookup_widget(widget,"imagen");  
  forma1=(GtkToggleButton *)lookup_widget(widget,"forma1");
  forma2=(GtkToggleButton *)lookup_widget(widget,"forma2");
  numObjeto =(GtkSpinButton *)lookup_widget(widget,"numObjeto");
  igualODiferente= (GtkLabel *)lookup_widget(widget,"igualODiferente");
  largoSeg =(GtkSpinButton *)lookup_widget(widget,"largoSeg");
  tolAngulo =(GtkSpinButton *)lookup_widget(widget,"tolAngulo");
  toleranciaComparaAngulo =(GtkSpinButton *)lookup_widget(widget,"toleranciaComparaAngulo");
  toleranciaCompara =(GtkSpinButton *)lookup_widget(widget,"toleranciaCompara");
}


void
on_numObjeto_value_changed             (GtkSpinButton   *spinbutton,
                                        gpointer         user_data)
{
  int actual=gtk_spin_button_get_value_as_int(numeroResultado)-1;
  int i;
  GdkPixbuf *pixbuf=imagenes[actual];
  guchar *pixels;
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  for (i=0;i<maximo[actual][NOBJETOS];i++){
    int x= objeto[actual][i][0];
    int y= objeto[actual][i][1];
    if (i== gtk_spin_button_get_value_as_int(numObjeto)-1)
      pintarRecursivo(x,y,1,0,pixels,-1,255,255,0);
     else
      pintarRecursivo(x,y,1,255,pixels,-1,255,0,0);
  }
  gdk_window_invalidate_region (gtk_widget_get_parent_window((GtkWidget *)imagen)  ,
			       gdk_drawable_get_visible_region ((GdkDrawable*)gtk_widget_get_parent_window((GtkWidget *)imagen) ),
			      FALSE);  
}


void
on_comparar_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  double segOrd[2][1000][2];
  int i;
  int actual,segActual;
  for (actual=0;actual<2;actual++){
    double perimetro=0;
    for (segActual=0;segActual<cantidadSegmentos[actual];segActual++){
      perimetro+=segmento[actual][segActual][LARGO];
    }
    for (segActual=0;segActual<cantidadSegmentos[actual];segActual++){
      segmento[actual][segActual][LARGO]/=perimetro;
    }
    double max=0;
    double ant=9999;
    for (i=0;i<cantidadSegmentos[actual];i++){
      double max=0;
      for (segActual=0;segActual<cantidadSegmentos[actual];segActual++){
        if((segmento[actual][segActual][LARGO]>=max)&&(segmento[actual][segActual][LARGO]<ant)){
          max=segOrd[actual][i][LARGO]=segmento[actual][segActual][LARGO];
          segOrd[actual][i][ANGULO]=segmento[actual][segActual][ANGULO];
        }
      }
      ant=max;
    }
    for (segActual=0;segActual<cantidadSegmentos[actual];segActual++){
      g_print ("\nForma: %d. Segmento Normalizado: %d. Angulo: %f. Longitud: %f.",actual,segActual,segOrd[actual][segActual][ANGULO],segOrd[actual][segActual][LARGO]);
    }

  }
  int ta=gtk_spin_button_get_value_as_int(toleranciaComparaAngulo);
  double tc=((double)gtk_spin_button_get_value_as_int(toleranciaCompara))/100;
  g_print ("\nta: %d. tc: %f.",ta,tc);
  if ( (abs((int)segOrd[0][0][ANGULO]-(int)segOrd[1][0][ANGULO])<ta)
     &&(labs(segOrd[0][0][LARGO]- segOrd[1][0][LARGO])<tc)
     &&(abs((int)segOrd[0][1][ANGULO]-(int)segOrd[1][1][ANGULO])<ta)
     &&(labs(segOrd[0][1][LARGO]- segOrd[1][1][LARGO])<tc))
     gtk_label_set_label(igualODiferente,"Iguales");
   else
     gtk_label_set_label(igualODiferente,"Distintos");

}
