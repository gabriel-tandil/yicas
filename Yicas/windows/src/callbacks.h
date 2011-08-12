#include <gtk/gtk.h>


void
on_window1_destroy                     (GtkObject       *object,
                                        gpointer         user_data);

void
on_window1_show                        (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_window1_realize                     (GtkWidget       *widget,
                                        gpointer         user_data);

gboolean
on_histograma_expose_event             (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

gboolean
on_muestra_expose_event                (GtkWidget       *widget,
                                        GdkEventExpose  *event,
                                        gpointer         user_data);

void
on_numeroResultado_value_changed       (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_procesar_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_separar_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_verOriginal_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_verResultados_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_selectorArchivo_selection_changed   (GtkFileChooser  *filechooser,
                                        gpointer         user_data);

void
on_matiz_clicked                       (GtkButton       *button,
                                        gpointer         user_data);

void
on_saturacion_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_valor_clicked                       (GtkButton       *button,
                                        gpointer         user_data);

void
on_valor_value_changed                 (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_borrarMaximo_clicked                (GtkButton       *button,
                                        gpointer         user_data);

void
on_valorMaximo_value_changed           (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);
