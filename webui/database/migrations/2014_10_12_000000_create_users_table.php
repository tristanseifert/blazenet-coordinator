<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    /**
     * Run the migrations.
     *
     * @return void
     */
    public function up()
    {
        Schema::create('users', function (Blueprint $table) {
            $table->id();
            // username (for logging in)
            $table->string('username')->unique();
            // email address (for notifications)
            $table->string('email')->nullable();
            $table->timestamp('email_verified_at')->nullable();
            // display name (optional)
            $table->string('display_name')->nullable();

            // password hash
            $table->string('password');
            // date after which the password expires
            $table->timestamp('password_expiration')->nullable();
            // last time the password was changed
            $table->timestamp('password_changed_on');
            // "remember me" cookie token
            $table->rememberToken();

            // whether the account is enabled
            $table->boolean('is_enabled')->default(true);

            // last login timestamp
            $table->timestamp('last_login')->nullable();
            // timestamps (modification/creation)
            $table->timestamps();
        });
    }

    /**
     * Reverse the migrations.
     *
     * @return void
     */
    public function down()
    {
        Schema::dropIfExists('users');
    }
};
