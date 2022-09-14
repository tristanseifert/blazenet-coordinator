<?php

namespace App\Providers;

use Illuminate\Pagination\Paginator;
use Illuminate\Support\ServiceProvider;

class ViewServiceProvider extends ServiceProvider
{
    /**
     * Register services.
     *
     * @return void
     */
    public function register()
    {
        //
    }

    /**
     * Bootstrap services.
     *
     * @return void
     */
    public function boot()
    {
        // register flash message severities
        \Spatie\Flash\Flash::levels([
            'info' => 'is-info',
            'success' => 'is-success',
            'warning' => 'is-warning',
            'error' => 'is-danger',
        ]);

        // configure pagination views
        Paginator::defaultView('components.pagination.default');
        Paginator::defaultSimpleView('components.pagination.simple');
    }
}
